#include "Roles.h"

namespace stdc
{

namespace
{
    // Why each part gets its engine (see README "Which engine for which drum"):
    //  - Bus:     feed-forward VCA bus compression is the classic "glue": low ratio, slow attack lets the hits
    //             through, programme-dependent (auto) release keeps it from pumping.
    //  - Kick:    RMS-sensing VCA with programme-dependent attack and a constant-rate (dB/s) release: holds the
    //             low end steady without distorting it, the beater still pokes through.
    //  - Snare:   fast feedback FET: grabs the stick in tens of microseconds and adds harmonic bite and crack.
    //  - Tom:     the same FET with a slower attack and medium release: keeps the stick, brings the ring up.
    //  - Cymbals: optical cell: slow, soft-kneed, programme-dependent levelling. No fast transient grab, so the
    //             wash doesn't swirl or pump.
    //  - Room:    FET with all ratios engaged ("all buttons"): over-compressing, distorting, pumping bloom.
    //  - Verb:    variable-mu tube: the ratio rises with level and the knee is very wide, so tails get dense and
    //             even without audible clamping.
    const std::array<AlgorithmInfo, kNumAlgorithms> algorithms {{
        { "Bus VCA",   "Clean glue, transients intact",
          "Feed-forward VCA with a blended peak/RMS detector. Clean and precise; Auto release adds a slow second "
          "stage so sustained compression recovers gently." },
        { "Punch VCA", "Tight RMS punch, steady lows",
          "Feed-forward VCA with a true RMS detector, programme-dependent attack (bigger jumps are caught faster) "
          "and a constant-rate release in dB per second." },
        { "FET",       "Fast, bright, aggressive bite",
          "Feedback FET design: very fast attack (down to 20 us), harder knee at high ratios and harmonics that grow "
          "with gain reduction." },
        { "FET All",   "Smashed, distorted, pumping",
          "FET with every ratio engaged: the curve over-compresses (louder input gets quieter), release speeds up and "
          "distortion rises sharply. Classic room-mic crush." },
        { "Opto",      "Smooth, slow, programme-dependent",
          "Optical feedback cell: soft wide knee, ratio that grows with level, and a two-stage release whose tail "
          "lengthens the longer it has been compressing." },
        { "Vari-Mu",   "Warm tube density",
          "Variable-mu tube feedback compressor: the ratio rises gradually with gain reduction (very soft knee) and "
          "the tube adds even harmonics as it works." },
    }};

    std::vector<RoleInfo> makeRoles()
    {
        using namespace ids;
        std::vector<RoleInfo> r;
        r.push_back ({ "Drum Bus", "Bus", Algorithm::busVca,
                       "Bus VCA: low ratio glue. A slow attack keeps the hits, Auto release keeps it from pumping.",
                       { { threshold, -18.0f }, { ratio, 4.0f }, { knee, 6.0f }, { range, 12.0f }, { attack, 10.0f },
                         { release, 200.0f }, { autoRelease, 1.0f }, { drive, 10.0f }, { character, 1.0f },
                         { mix, 100.0f }, { scHighPass, 90.0f }, { stereoLink, 100.0f }, { autoGain, 1.0f } } });
        r.push_back ({ "Kick", "Kick", Algorithm::punchVca,
                       "Punch VCA: RMS sensing keeps the low end steady; the beater gets through the programme-dependent attack.",
                       { { threshold, -20.0f }, { ratio, 4.0f }, { knee, 3.0f }, { range, 18.0f }, { attack, 12.0f },
                         { release, 90.0f }, { autoRelease, 0.0f }, { drive, 15.0f }, { character, 2.0f },
                         { mix, 100.0f }, { scHighPass, 20.0f }, { stereoLink, 100.0f }, { autoGain, 1.0f } } });
        r.push_back ({ "Snare", "Snare", Algorithm::fet,
                       "FET: catches the stick fast and adds crack and bite.",
                       { { threshold, -24.0f }, { ratio, 4.0f }, { knee, 3.0f }, { range, 20.0f }, { attack, 1.5f },
                         { release, 70.0f }, { autoRelease, 0.0f }, { drive, 15.0f }, { character, 0.0f },
                         { mix, 100.0f }, { scHighPass, 60.0f }, { stereoLink, 100.0f }, { autoGain, 1.0f } } });
        r.push_back ({ "Tom", "Tom", Algorithm::fet,
                       "FET with a slower attack: the stick stays, the ring comes up.",
                       { { threshold, -26.0f }, { ratio, 6.0f }, { knee, 4.0f }, { range, 18.0f }, { attack, 8.0f },
                         { release, 140.0f }, { autoRelease, 0.0f }, { drive, 10.0f }, { character, 0.0f },
                         { mix, 100.0f }, { scHighPass, 50.0f }, { stereoLink, 100.0f }, { autoGain, 1.0f } } });
        r.push_back ({ "Cymbals", "Cym", Algorithm::opto,
                       "Opto: slow, soft levelling with no transient grab, so the wash doesn't swirl or pump.",
                       { { threshold, -22.0f }, { ratio, 3.0f }, { knee, 12.0f }, { range, 9.0f }, { attack, 10.0f },
                         { release, 900.0f }, { autoRelease, 0.0f }, { drive, 0.0f }, { character, 1.0f },
                         { mix, 100.0f }, { scHighPass, 160.0f }, { stereoLink, 100.0f }, { autoGain, 1.0f } } });
        r.push_back ({ "Room", "Room", Algorithm::fetAll,
                       "FET All: crushed, distorted, pumping room. Blend it in with Mix.",
                       { { threshold, -30.0f }, { ratio, 20.0f }, { knee, 0.0f }, { range, 36.0f }, { attack, 0.1f },
                         { release, 60.0f }, { autoRelease, 0.0f }, { drive, 25.0f }, { character, 0.0f },
                         { mix, 70.0f }, { scHighPass, 40.0f }, { stereoLink, 100.0f }, { autoGain, 1.0f } } });
        r.push_back ({ "Verb", "Verb", Algorithm::variMu,
                       "Vari-Mu: ratio rises with level for dense, even reverb tails.",
                       { { threshold, -26.0f }, { ratio, 3.0f }, { knee, 10.0f }, { range, 14.0f }, { attack, 25.0f },
                         { release, 350.0f }, { autoRelease, 1.0f }, { drive, 10.0f }, { character, 0.0f },
                         { mix, 100.0f }, { scHighPass, 120.0f }, { stereoLink, 100.0f }, { autoGain, 1.0f } } });
        return r;
    }
}

const RoleInfo& getRoleInfo (int role)
{
    static const auto roles = makeRoles();
    return roles[(size_t) juce::jlimit (0, kNumRoles - 1, role)];
}

const AlgorithmInfo& getAlgorithmInfo (int algorithm)
{
    return algorithms[(size_t) juce::jlimit (0, kNumAlgorithms - 1, algorithm)];
}

juce::StringArray getRoleNames()
{
    juce::StringArray s;
    for (int i = 0; i < kNumRoles; ++i) s.add (getRoleInfo (i).name);
    return s;
}

juce::StringArray getEngineChoiceNames()
{
    juce::StringArray s { "By Drum Part" };
    for (int i = 0; i < kNumAlgorithms; ++i) s.add (getAlgorithmInfo (i).name);
    return s;
}

} // namespace stdc
