#include "Compressor.h"

namespace stdc
{

namespace
{
    inline float smoothstep01 (float x) noexcept
    {
        x = juce::jlimit (0.0f, 1.0f, x);
        return x * x * (3.0f - 2.0f * x);
    }

    /** Polynomial smooth minimum (exactly min() further than k away from the crossover). */
    inline float smoothMin (float a, float b, float k) noexcept
    {
        const float h = juce::jlimit (0.0f, 1.0f, 0.5f + 0.5f * (b - a) / k);
        return b + (a - b) * h - k * h * (1.0f - h);
    }

    /** Overshoot above the threshold through a quadratic soft knee of width `knee` (dB, >= 0). */
    inline float kneeOver (float over, float knee) noexcept
    {
        if (knee <= 0.01f) return juce::jmax (0.0f, over);
        const float h = knee * 0.5f;
        if (over <= -h) return 0.0f;
        if (over >= h) return over;
        const float t = over + h;
        return t * t / (2.0f * knee);
    }

    /** dB slope of the gain change above the threshold: 1/R - 1 (R >= 100 = infinity). */
    inline float slopeFor (float ratio) noexcept
    {
        return ratio >= 99.5f ? -1.0f : 1.0f / juce::jmax (1.0f, ratio) - 1.0f;
    }

    inline bool isFeedback (Algorithm a) noexcept
    {
        return a == Algorithm::fet || a == Algorithm::opto || a == Algorithm::variMu;
    }

    /** Each engine's knee: optical and tube engines are always soft, FET All is harder. */
    inline float engineKnee (Algorithm a, float knee) noexcept
    {
        switch (a)
        {
            case Algorithm::opto:   return knee + 8.0f;
            case Algorithm::variMu: return knee + 14.0f;
            case Algorithm::fetAll: return knee * 0.5f;
            case Algorithm::busVca:
            case Algorithm::punchVca:
            case Algorithm::fet:
            default:                return knee;
        }
    }

    /** Gain change (dB <= 0) for a knee-shaped overshoot `o` and slope `b`, with the engine's curve shape. */
    inline float engineCurve (Algorithm a, float o, float b) noexcept
    {
        switch (a)
        {
            case Algorithm::opto:     // the ratio grows with level (a cell that darkens more the harder it's lit)
                return b * o * (0.55f + 0.45f * smoothstep01 (o / 18.0f));
            case Algorithm::variMu:   // the mu tube's ratio rises gradually with gain reduction
                return b * o * (0.25f + 0.75f * smoothstep01 (o / 24.0f));
            case Algorithm::fetAll:   // over-compression: past the threshold louder gets quieter, then it settles
                return b * (1.18f * juce::jmin (o, 15.0f) + juce::jmax (0.0f, o - 15.0f));
            case Algorithm::busVca:
            case Algorithm::punchVca:
            case Algorithm::fet:
            default:
                return b * o;
        }
    }

    inline float limitRange (float gr, float rangeDb) noexcept
    {
        if (rangeDb >= 47.5f) return gr;
        const float r = juce::jmax (0.0f, rangeDb);
        if (r <= 0.0f) return 0.0f;
        return -smoothMin (-gr, r, 1.5f);
    }

    /** Feed-forward curve on the input level (what the knee display shows). */
    inline float feedForwardGr (float inDb, const CompSettings& s) noexcept
    {
        const float o = kneeOver (inDb - s.thresholdDb, engineKnee (s.algorithm, s.kneeDb));
        return limitRange (engineCurve (s.algorithm, o, slopeFor (s.ratio)), s.rangeDb);
    }

    /** Feedback engines detect their own output y. With a slope b' = b / (1 + b) on y, the steady state
        (gr = b' (x + gr - T)) has the slope b on the input, i.e. the same ratio as the knob says. Feedback can't
        reach infinity: the ratio is capped (~25:1), like the hardware. */
    inline float feedbackGr (float outDb, const CompSettings& s) noexcept
    {
        const float b = juce::jmax (-0.96f, slopeFor (s.ratio));
        const float bf = b / (1.0f + b);
        const float o = kneeOver (outDb - s.thresholdDb, engineKnee (s.algorithm, s.kneeDb));
        return limitRange (engineCurve (s.algorithm, o, bf), s.rangeDb);
    }
}

//==============================================================================
float Compressor::staticGainDb (float inDb, const CompSettings& s) noexcept
{
    if (! isFeedback (s.algorithm)) return feedForwardGr (inDb, s);

    // Steady state of a feedback design: h = fb (x + h). q(h) = h - fb (x + h) increases monotonically (fb' <= 0),
    // so bisection always finds it.
    float lo = -90.0f, hi = 0.0f;
    for (int i = 0; i < 26; ++i)
    {
        const float mid = 0.5f * (lo + hi);
        if (mid - feedbackGr (inDb + mid, s) > 0.0f) hi = mid; else lo = mid;
    }
    return 0.5f * (lo + hi);
}

float Compressor::autoMakeupDb (const CompSettings& s) noexcept
{
    // Half the gain reduction a loud drum hit (-6 dBFS detector level) would get: close to the loudness lost on
    // typical drum material, without chasing the programme.
    return juce::jlimit (0.0f, 24.0f, -0.5f * staticGainDb (-6.0f, s));
}

float Compressor::colourAmount (Algorithm a, float grDb) noexcept
{
    const float g = juce::jmax (0.0f, -grDb);
    float c = 0.0f;
    switch (a)
    {
        case Algorithm::busVca:   c = 0.02f + 0.004f * g; break;
        case Algorithm::punchVca: c = 0.03f + 0.006f * g; break;
        case Algorithm::fet:      c = 0.05f + 0.012f * g; break;
        case Algorithm::fetAll:   c = 0.16f + 0.020f * g; break;
        case Algorithm::opto:     c = 0.04f + 0.006f * g; break;
        case Algorithm::variMu:   c = 0.06f + 0.014f * g; break;
    }
    return juce::jmin (0.6f, c);
}

//==============================================================================
void Compressor::prepare (double sampleRate, int maxBlockSize)
{
    fs = sampleRate > 0.0 ? sampleRate : 48000.0;
    for (auto& g : gains) g.assign ((size_t) juce::jmax (1, maxBlockSize), 0.0f);
    setSettings (settings);
    reset();
}

void Compressor::reset() noexcept
{
    meanSq.fill (0.0f);
    gr.fill (0.0f);
    grSlow.fill (0.0f);
    grOpto.fill (0.0f);
    prevGain.fill (1.0f);
    optoMemory = 0.0f;
    detectorDb = -120.0f;
}

float Compressor::coeff (float ms) const noexcept
{
    return ms <= 0.0f ? 1.0f : (float) (1.0 - std::exp (-1.0 / ((double) ms * 0.001 * fs)));
}

void Compressor::setSettings (const CompSettings& s) noexcept
{
    settings = s;
    feedback = isFeedback (s.algorithm);

    float attack = s.attackMs, release = s.releaseMs, rmsMs = 5.0f;
    switch (s.algorithm)
    {
        case Algorithm::busVca:   rmsBlend = 0.5f; rmsMs = 5.0f; break;
        case Algorithm::punchVca: rmsBlend = 1.0f; rmsMs = 8.0f; break;
        case Algorithm::fet:      rmsBlend = 0.0f; attack = juce::jmax (0.02f, attack); break;
        case Algorithm::fetAll:   rmsBlend = 0.0f; attack = juce::jmax (0.1f, attack); release *= 0.6f; break;
        case Algorithm::opto:     rmsBlend = 0.6f; rmsMs = 10.0f; attack = juce::jmax (2.0f, attack); break;
        case Algorithm::variMu:   rmsBlend = 0.6f; rmsMs = 6.0f; attack = juce::jmax (1.0f, attack); break;
    }

    attCoeff = coeff (attack);
    relCoeff = coeff (release);
    rmsCoeff = coeff (rmsMs);
    // auto release: a fast stage recovers short hits, a slow stage holds sustained compression
    fastRelCoeff = coeff (juce::jmax (10.0f, release * 0.35f));
    slowAttCoeff = coeff (juce::jlimit (150.0f, 1500.0f, release * 10.0f));
    slowRelCoeff = coeff (release * 3.0f);
    // opto: fast part of the release (~50% of the reduction) and slower attack of the slow part
    optoFastRel = coeff (juce::jlimit (40.0f, 90.0f, release * 0.06f));
    optoSlowAtt = coeff (attack * 3.0f);
    // punch VCA: constant-rate release, `release` = time to recover 10 dB
    releaseRateDbPerSample = (float) (10.0 / ((double) juce::jmax (1.0f, release) * 0.001 * fs));
}

float Compressor::targetGr (float levelDb) const noexcept
{
    return feedback ? feedbackGr (levelDb, settings) : feedForwardGr (levelDb, settings);
}

void Compressor::computeGain (const float* const* detector, int numCh, int n) noexcept
{
    numCh = juce::jlimit (1, kMaxChannels, numCh);
    n = juce::jmin (n, (int) gains[0].size());
    const auto algo = settings.algorithm;
    const bool autoRel = settings.autoRelease && algo != Algorithm::opto;
    const float link = juce::jlimit (0.0f, 1.0f, settings.stereoLink);
    const float dt = (float) (1.0 / fs);

    // opto: the slow tail lengthens with how long / hard the cell has been lit (memory, per block)
    const float optoSlowRel = coeff (settings.releaseMs * (1.0f + optoMemory * (settings.autoRelease ? 2.0f : 1.0f)));

    float peakIn = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        std::array<float, kMaxChannels> lin {};
        float maxLin = 0.0f;
        for (int c = 0; c < numCh; ++c)
        {
            float x = detector[c][i];
            peakIn = juce::jmax (peakIn, std::abs (x));
            if (feedback) x *= prevGain[(size_t) c];
            float& m = meanSq[(size_t) c];
            m += rmsCoeff * (x * x - m);
            if (m < 1.0e-20f) m = 0.0f;
            const float a = std::abs (x);
            lin[(size_t) c] = a + rmsBlend * (std::sqrt (m) - a);
            maxLin = juce::jmax (maxLin, lin[(size_t) c]);
        }

        float outMax = 0.0f;
        for (int c = 0; c < numCh; ++c)
        {
            const float l = lin[(size_t) c] + link * (maxLin - lin[(size_t) c]);
            const float t = targetGr (st::dsp::gainToDb (l, -120.0f));
            float& g = gr[(size_t) c];
            float out;

            if (algo == Algorithm::punchVca && ! autoRel)
            {
                if (t < g)
                {
                    // programme-dependent attack: bigger jumps are caught faster
                    const float k = juce::jlimit (0.5f, 4.0f, (g - t) / 10.0f);
                    g += (1.0f - std::pow (1.0f - attCoeff, k)) * (t - g);
                }
                else g = juce::jmin (t, g + releaseRateDbPerSample);
                out = g;
            }
            else if (algo == Algorithm::opto)
            {
                g += (t < g ? attCoeff : optoFastRel) * (t - g);
                float& s = grOpto[(size_t) c];
                s += (t < s ? optoSlowAtt : optoSlowRel) * (t - s);
                out = 0.5f * (g + s);
            }
            else
            {
                g += (t < g ? attCoeff : (autoRel ? fastRelCoeff : relCoeff)) * (t - g);
                out = g;
                if (autoRel)
                {
                    float& s = grSlow[(size_t) c];
                    s += (t < s ? slowAttCoeff : slowRelCoeff) * (t - s);
                    out = juce::jmin (g, s);
                }
            }

            if (out > -1.0e-6f) out = 0.0f;
            gains[(size_t) c][(size_t) i] = out;
            prevGain[(size_t) c] = st::dsp::dbToGain (out);
            outMax = juce::jmin (outMax, out);
        }

        if (algo == Algorithm::opto)
        {
            const float depth = -outMax;
            optoMemory += dt * (depth > 2.0f ? depth / 12.0f : 0.0f) - dt * optoMemory / 4.0f;
            optoMemory = juce::jlimit (0.0f, 3.0f, optoMemory);
        }
    }

    for (int c = numCh; c < kMaxChannels; ++c)
        std::copy (gains[0].begin(), gains[0].begin() + n, gains[(size_t) c].begin());
    detectorDb = st::dsp::gainToDb (peakIn, -120.0f);
}

} // namespace stdc
