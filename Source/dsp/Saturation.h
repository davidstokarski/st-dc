#pragma once
#include "../Params.h"

namespace stdc
{

/** Post-compression colour, run at the oversampled rate:
    1. engine colour: the harmonics that come with the selected engine, growing with gain reduction
       (y = x + c (f(x) - x), tiny at 0 dB GR),
    2. Drive: the user's saturation, character Tube (asymmetric, even harmonics), Tape (soft, symmetric, HF
       roll-off with drive) or Transformer (low end driven harder, then restored). The drive is normalised so a
       -6 dBFS hit keeps its peak level: more drive = denser, louder body with held peaks.
    A DC blocker follows (asymmetric curves add DC). Audio thread only after prepare(). */
class Saturator
{
public:
    static constexpr int kMaxChannels = 2;

    void prepare (double sampleRate, int numChannels);
    void reset() noexcept;

    /** colourFrom/To: engine colour amount ramped across the block; asymmetric: tube-like (even) engine colour. */
    void process (float* const* data, int numCh, int n, float drive01, int character,
                  float colourFrom, float colourTo, bool asymmetric) noexcept;

    /** Level compensation (dB) Auto Gain applies for a drive amount (the drive adds body / loudness). */
    static float driveCompensationDb (float drive01) noexcept { return -0.45f * drive01 * 24.0f; }

    /** Static curve of the Drive stage for one character at a drive amount (for tests / UI). */
    static float shape (int character, float x) noexcept;

private:
    double fs = 48000.0;
    st::dsp::OnePole tapeLp, xfmrPre, xfmrPost;
    st::dsp::DcBlocker dc;
    float lastTapeHz = -1.0f;
};

} // namespace stdc
