#include "Saturation.h"

namespace stdc
{

namespace
{
    constexpr float kTubeBias = 0.3f;
    const float kTubeNorm = 1.0f / (1.0f - std::tanh (kTubeBias) * std::tanh (kTubeBias));   // 1 / sech^2(bias)

    /** Asymmetric tube-like curve, unity slope at 0, no DC at 0. */
    inline float tube (float x) noexcept { return (std::tanh (x + kTubeBias) - std::tanh (kTubeBias)) * kTubeNorm; }
    inline float tapeCurve (float x) noexcept { return x / std::pow (1.0f + std::pow (std::abs (x), 2.2f), 1.0f / 2.2f); }
}

float Saturator::shape (int character, float x) noexcept
{
    switch (character)
    {
        case 0:  return tube (x);
        case 1:  return tapeCurve (x);
        default: return std::tanh (x);
    }
}

void Saturator::prepare (double sampleRate, int numChannels)
{
    fs = sampleRate > 0.0 ? sampleRate : 48000.0;
    tapeLp.setType (st::dsp::OnePoleType::lowPass);
    tapeLp.prepare (fs, numChannels);
    tapeLp.setCutoff (juce::jmin (20000.0, fs * 0.45));
    lastTapeHz = -1.0f;
    for (auto* f : { &xfmrPre, &xfmrPost })
    {
        f->setType (st::dsp::OnePoleType::lowPass);
        f->prepare (fs, numChannels);
        f->setCutoff (150.0);
    }
    dc.prepare (fs, numChannels, 8.0);
    reset();
}

void Saturator::reset() noexcept
{
    tapeLp.reset();
    xfmrPre.reset();
    xfmrPost.reset();
    dc.reset();
}

void Saturator::process (float* const* data, int numCh, int n, float drive01, int character,
                         float colourFrom, float colourTo, bool asymmetric) noexcept
{
    numCh = juce::jlimit (1, kMaxChannels, numCh);
    drive01 = juce::jlimit (0.0f, 1.0f, drive01);
    const bool driving = drive01 > 0.0005f;

    // Drive: g into the curve, normalised so a 0.5 (-6 dBFS) peak keeps its level; blended in over the first 1/6
    const float g = st::dsp::dbToGain (drive01 * 24.0f);
    const float ref = 0.5f;
    const float norm = ref / shape (character == 0 ? 2 : character, g * ref);   // positive side of every curve ~ tanh
    const float wet = juce::jmin (1.0f, drive01 * 6.0f);

    if (character == 1 && driving)
    {
        const float hz = (float) juce::jmin (fs * 0.45, 20000.0 - 12000.0 * (double) drive01);
        if (std::abs (hz - lastTapeHz) > 1.0f) { tapeLp.setCutoff (hz); lastTapeHz = hz; }
    }

    const float step = n > 1 ? (colourTo - colourFrom) / (float) n : 0.0f;
    for (int c = 0; c < numCh; ++c)
    {
        float* d = data[c];
        float colour = colourFrom;
        for (int i = 0; i < n; ++i, colour += step)
        {
            float x = d[i];

            // 1. engine colour
            const float f = asymmetric ? tube (x) : std::tanh (x);
            x += colour * (f - x);

            // 2. drive
            if (driving)
            {
                float y;
                switch (character)
                {
                    case 0:  y = tube (g * x) * norm; break;
                    case 1:  y = tapeLp.processSample (c, tapeCurve (g * x) * norm); break;
                    default:
                    {
                        // transformer: lows driven ~6 dB harder, then the same amount taken back off
                        const float pre = x + xfmrPre.processSample (c, x);
                        const float s = std::tanh (g * pre) * norm;
                        y = s - 0.5f * xfmrPost.processSample (c, s);
                        break;
                    }
                }
                x += wet * (y - x);
            }

            d[i] = dc.processSample (c, x);
        }
    }
}

} // namespace stdc
