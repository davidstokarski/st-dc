#pragma once
#include "../Roles.h"

namespace stdc
{

/** Settings of the gain computer (updated per chunk on the audio thread; cheap). */
struct CompSettings
{
    Algorithm algorithm = Algorithm::busVca;
    float thresholdDb = -18.0f;
    float ratio = 4.0f;          // >= 100 = infinity
    float kneeDb = 6.0f;
    float rangeDb = 24.0f;       // >= 48 = unlimited
    float attackMs = 10.0f;
    float releaseMs = 150.0f;
    bool autoRelease = false;
    float stereoLink = 1.0f;     // 0..1
};

/** The six compression engines of ST DC, one class: detector -> gain computer -> ballistics, per sample, up to
    two channels, feed-forward or feedback depending on the engine (see AlgorithmInfo for what each one does).

    Everything happens in the dB domain on the gain reduction (gr <= 0). The static curve (staticGainDb) is what
    the knee display draws; feedback engines solve their output-detecting curve so that their steady state lands
    on the same curve.

    Threading: prepare() on the message thread, the rest on the audio thread (no allocation). */
class Compressor
{
public:
    static constexpr int kMaxChannels = 2;

    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;
    void setSettings (const CompSettings&) noexcept;
    const CompSettings& getSettings() const noexcept { return settings; }

    /** Computes the gain (dB, <= 0) for n samples from `detector` (numCh channels) and writes it to gainDb[ch][i].
        Feedback engines need the audio itself (their detector sees the compressed signal): they use detector *
        their own previous gain. */
    void computeGain (const float* const* detector, int numCh, int n) noexcept;
    const float* getGainDb (int ch) const noexcept { return gains[(size_t) juce::jlimit (0, kMaxChannels - 1, ch)].data(); }

    /** Static transfer: gain change (dB, <= 0) for a steady detector level `inDb`. Any thread. */
    static float staticGainDb (float inDb, const CompSettings&) noexcept;

    /** Programme-independent estimate of the make-up gain (dB) for Auto Gain. */
    static float autoMakeupDb (const CompSettings&) noexcept;

    /** Engine colour (saturation that comes with the engine) for a gain reduction of grDb: 0..~0.5. */
    static float colourAmount (Algorithm, float grDb) noexcept;

    /** Highest detector level of the last block (dB), for the knee display. */
    float getDetectorDb() const noexcept { return detectorDb; }

private:
    CompSettings settings;
    double fs = 48000.0;

    // per-channel state
    std::array<float, kMaxChannels> meanSq {}, gr {}, grSlow {}, grOpto {}, prevGain { 1.0f, 1.0f };
    float optoMemory = 0.0f;
    float detectorDb = -120.0f;
    std::array<std::vector<float>, kMaxChannels> gains;

    // derived per settings
    float attCoeff = 1.0f, relCoeff = 1.0f, rmsCoeff = 1.0f, rmsBlend = 0.0f;
    float slowAttCoeff = 1.0f, slowRelCoeff = 1.0f, fastRelCoeff = 1.0f, optoFastRel = 1.0f, optoSlowAtt = 1.0f;
    float releaseRateDbPerSample = 0.0f;   // Punch VCA linear release
    bool feedback = false;

    float coeff (float ms) const noexcept;
    float targetGr (float levelDb) const noexcept;
};

} // namespace stdc
