#include "Engine.h"

namespace stdc
{

void Engine::prepare (double sampleRate, int maxBlockSize, int numChannels)
{
    fs = sampleRate > 0.0 ? sampleRate : 48000.0;
    maxBlock = juce::jmax (1, maxBlockSize);
    channels = juce::jlimit (1, 2, numChannels);

    oversampler.prepare (fs, maxBlock, channels);
    oversampler.setFactorIndex (quality);
    latency = oversampler.getLatencySamples (quality);

    int maxLatency = 0;
    for (int q = 0; q < 3; ++q) maxLatency = juce::jmax (maxLatency, oversampler.getLatencySamples (q));
    dryDelay.prepare (channels, maxLatency + 1);
    dryDelay.setDelay (latency);

    comp.prepare (fs, maxBlock);
    saturator.prepare (fs * (double) st::dsp::Oversampler::factorForIndex (quality), channels);
    for (auto& f : scFilter) f.prepare (channels);
    lastHpf = -1.0f;

    dry.setSize (channels, maxBlock, false, true, true);
    inGainRamp.assign ((size_t) maxBlock, 1.0f);
    detector.setSize (channels, maxBlock, false, true, true);

    for (auto* s : { &inGain, &outGain, &makeup, &mixAmount, &bypassAmount })
        s->prepare (fs, 20.0);

    frameLength = juce::jmax (16, juce::roundToInt (fs * 0.004));
    history.setFrameRate (fs / frameLength);
    prepared = true;
    setSettings (settings);
    reset();
}

void Engine::reset() noexcept
{
    comp.reset();
    saturator.reset();
    oversampler.reset();
    dryDelay.reset();
    for (auto& f : scFilter) f.reset();
    inGain.setCurrentAndTarget (st::dsp::dbToGain (settings.inputDb));
    outGain.setCurrentAndTarget (st::dsp::dbToGain (settings.outputDb));
    mixAmount.setCurrentAndTarget (settings.mix);
    bypassAmount.setCurrentAndTarget (settings.bypass ? 1.0f : 0.0f);
    makeup.setCurrentAndTarget (st::dsp::dbToGain (makeupDb.load()));
    lastColour = 0.0f;
    frameCount = 0;
    frameIn = frameOut = frameGr = 0.0f;
}

void Engine::setQuality (int index)
{
    quality = juce::jlimit (0, 2, index);
    if (! prepared) return;
    oversampler.setFactorIndex (quality);
    latency = oversampler.getLatencySamples (quality);
    dryDelay.setDelay (latency);
    saturator.prepare (fs * (double) st::dsp::Oversampler::factorForIndex (quality), channels);
    oversampler.reset();
    dryDelay.reset();
}

void Engine::updateSidechainFilter() noexcept
{
    const float hz = settings.scHighPassHz;
    if (std::abs (hz - lastHpf) < 0.01f) return;
    lastHpf = hz;
    for (auto& f : scFilter)
        f.setParameters (st::dsp::BiquadType::highPass, fs, juce::jlimit (20.0, fs * 0.45, (double) hz), 0.7071);
}

void Engine::setSettings (const EngineSettings& s) noexcept
{
    settings = s;
    comp.setSettings (s.comp);
    inGain.setTarget (st::dsp::dbToGain (s.inputDb));
    outGain.setTarget (st::dsp::dbToGain (s.outputDb));
    mixAmount.setTarget (juce::jlimit (0.0f, 1.0f, s.mix));
    bypassAmount.setTarget (s.bypass ? 1.0f : 0.0f);

    float mk = 0.0f;
    if (s.autoGain)
        mk = Compressor::autoMakeupDb (s.comp) + Saturator::driveCompensationDb (s.drive);
    makeupDb.store (mk, std::memory_order_relaxed);
    makeup.setTarget (st::dsp::dbToGain (mk));
    updateSidechainFilter();
}

void Engine::process (juce::AudioBuffer<float>& main, const juce::AudioBuffer<float>* sidechain, int n) noexcept
{
    n = juce::jmin (n, maxBlock);
    const int numCh = juce::jmin (channels, main.getNumChannels());
    if (n <= 0 || numCh <= 0) return;

    // ---- input gain; keep the raw input (bypass) in `dry`, latency-aligned with the oversampled path
    for (int c = 0; c < numCh; ++c) dry.copyFrom (c, 0, main, c, 0, n);
    {
        float* d[2] = { main.getWritePointer (0), numCh > 1 ? main.getWritePointer (1) : nullptr };
        for (int i = 0; i < n; ++i)
        {
            const float g = inGainRamp[(size_t) i] = inGain.getNext();
            for (int c = 0; c < numCh; ++c) d[c][i] *= g;
        }
    }
    for (int c = 0; c < numCh; ++c) dryDelay.processChannel (c, dry.getWritePointer (c), n);
    dryDelay.advance (n);

    float inPeak = 0.0f;
    for (int c = 0; c < numCh; ++c) inPeak = juce::jmax (inPeak, main.getMagnitude (c, 0, n));

    // ---- detector: main signal or the side-chain input, high-passed
    const bool ext = settings.externalSidechain && sidechain != nullptr && sidechain->getNumChannels() > 0
                     && sidechain->getNumSamples() >= n;
    for (int c = 0; c < numCh; ++c)
    {
        if (ext) detector.copyFrom (c, 0, *sidechain, juce::jmin (c, sidechain->getNumChannels() - 1), 0, n);
        else detector.copyFrom (c, 0, main, c, 0, n);
    }
    if (settings.scHighPassHz > 20.5f)
        for (int c = 0; c < numCh; ++c)
        {
            float* d = detector.getWritePointer (c);
            for (int i = 0; i < n; ++i) d[i] = scFilter[1].processSample (c, scFilter[0].processSample (c, d[i]));
        }

    // ---- gain computer + ballistics (base rate), applied before the colour stage
    comp.computeGain (detector.getArrayOfReadPointers(), numCh, n);
    float grMin = 0.0f, grSum = 0.0f;
    for (int c = 0; c < numCh; ++c)
    {
        float* d = main.getWritePointer (c);
        const float* g = comp.getGainDb (c);
        for (int i = 0; i < n; ++i)
        {
            d[i] *= st::dsp::dbToGain (g[i]);
            grMin = juce::jmin (grMin, g[i]);
            grSum += g[i];
        }
    }
    const float grMean = grSum / (float) (n * numCh);

    // ---- oversampled colour: engine harmonics + drive
    {
        const auto algo = settings.comp.algorithm;
        const float colour = Compressor::colourAmount (algo, grMean);
        const bool asym = algo == Algorithm::fet || algo == Algorithm::fetAll || algo == Algorithm::opto || algo == Algorithm::variMu;
        float* ptrs[2] = { main.getWritePointer (0), numCh > 1 ? main.getWritePointer (1) : nullptr };
        juce::dsp::AudioBlock<float> block (ptrs, (size_t) numCh, (size_t) n);
        auto up = oversampler.processUp (block);
        float* upPtrs[2] = { up.getChannelPointer (0), numCh > 1 ? up.getChannelPointer (1) : nullptr };
        saturator.process (upPtrs, numCh, (int) up.getNumSamples(), settings.drive, settings.character, lastColour, colour, asym);
        oversampler.processDown (block);
        lastColour = colour;
    }

    // ---- make-up, mix, output, bypass
    float outPeak = 0.0f;
    {
        float* d[2] = { main.getWritePointer (0), numCh > 1 ? main.getWritePointer (1) : nullptr };
        const float* raw[2] = { dry.getReadPointer (0), numCh > 1 ? dry.getReadPointer (1) : nullptr };
        for (int i = 0; i < n; ++i)
        {
            const float mk = makeup.getNext(), mx = mixAmount.getNext(), og = outGain.getNext(), bp = bypassAmount.getNext();
            const float ig = inGainRamp[(size_t) i];   // the Mix dry signal has the input gain too
            for (int c = 0; c < numCh; ++c)
            {
                const float wet = d[c][i] * mk;
                float y = (raw[c][i] * ig + mx * (wet - raw[c][i] * ig)) * og;
                y += bp * (raw[c][i] - y);
                d[c][i] = y;
                outPeak = juce::jmax (outPeak, std::abs (y));
            }
        }
    }

    // ---- meters + history
    grDb.store (grMin, std::memory_order_relaxed);
    detectorDb.store (comp.getDetectorDb(), std::memory_order_relaxed);
    frameIn = juce::jmax (frameIn, inPeak);
    frameOut = juce::jmax (frameOut, outPeak);
    frameGr = juce::jmin (frameGr, grMin);
    frameCount += n;
    while (frameCount >= frameLength)
    {
        frameCount -= frameLength;
        history.push ({ st::dsp::gainToDb (frameIn, -120.0f), st::dsp::gainToDb (frameOut, -120.0f), frameGr });
        frameIn = frameOut = frameGr = 0.0f;
    }
}

} // namespace stdc
