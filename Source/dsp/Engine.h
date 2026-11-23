#pragma once
#include "Compressor.h"
#include "Saturation.h"

namespace stdc
{

struct EngineSettings
{
    CompSettings comp;
    float inputDb = 0.0f, outputDb = 0.0f;
    bool autoGain = false;
    float drive = 0.0f;          // 0..1
    int character = 0;
    float mix = 1.0f;            // 0..1
    float scHighPassHz = 20.0f;  // <= 20.5 = off
    bool externalSidechain = false;
    bool bypass = false;
};

/** ST DC's whole signal path, independent of AudioProcessor (offline testable):

      input gain -> [detector: main or side-chain input -> side-chain high-pass] -> Compressor gain (base rate)
      -> oversample -> engine colour + Drive -> downsample -> auto / make-up gain
      -> Mix with the (latency-aligned) dry signal -> output gain -> bypass cross-fade

    prepare()/setQuality() on the message thread; process() in chunks of <= maxBlock on the audio thread. */
class Engine
{
public:
    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset() noexcept;

    /** 0 = off, 1 = 2x, 2 = 4x. Re-prepares: message thread, processing suspended. */
    void setQuality (int index);
    int getQuality() const noexcept { return quality; }
    int getLatencySamples() const noexcept { return latency; }

    void setSettings (const EngineSettings&) noexcept;
    const EngineSettings& getSettings() const noexcept { return settings; }

    /** Processes `main` in place (n <= maxBlockSize). `sidechain` may be null or have 0 channels. */
    void process (juce::AudioBuffer<float>& main, const juce::AudioBuffer<float>* sidechain, int n) noexcept;

    // Meters (any thread)
    std::atomic<float> grDb { 0.0f };          // deepest gain reduction of the last block
    std::atomic<float> detectorDb { -120.0f }; // detector (input) peak of the last block
    std::atomic<float> makeupDb { 0.0f };      // current make-up gain (auto)
    st::LevelHistory history;   // what the displays read (gain change <= 0 here)

    const Compressor& getCompressor() const noexcept { return comp; }

private:
    EngineSettings settings;
    Compressor comp;
    Saturator saturator;
    st::dsp::Oversampler oversampler;
    st::dsp::DelayLine dryDelay;
    std::array<st::dsp::Biquad, 2> scFilter;   // 12 dB/oct x2 = 24 dB/oct side-chain high-pass
    st::dsp::LinearSmoother inGain, outGain, makeup, mixAmount, bypassAmount;

    juce::AudioBuffer<float> dry, detector;
    std::vector<float> inGainRamp;
    double fs = 48000.0;
    int maxBlock = 64, channels = 2, quality = 0, latency = 0;
    float lastHpf = -1.0f, lastColour = 0.0f;
    bool prepared = false;

    // history frame accumulation
    int frameLength = 192, frameCount = 0;
    float frameIn = 0.0f, frameOut = 0.0f, frameGr = 0.0f, frameDet = -120.0f;

    void updateSidechainFilter() noexcept;
};

} // namespace stdc
