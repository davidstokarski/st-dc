#pragma once
#include "Params.h"

namespace stdc
{

/** The parts of a drum kit ST DC knows about. The order is permanent (it is the `role` parameter's value). */
enum class Role { bus, kick, snare, tom, cymbals, room, verb };
constexpr int kNumRoles = 7;

/** The compression engines. The order is permanent (`engine` parameter value - 1). */
enum class Algorithm { busVca, punchVca, fet, fetAll, opto, variMu };
constexpr int kNumAlgorithms = 6;

struct AlgorithmInfo
{
    const char* name;          // shown in the UI ("FET")
    const char* character;     // one line: what it sounds like
    const char* detail;        // tooltip: how it works
};

struct RoleInfo
{
    const char* name;          // "Drum Bus"
    const char* shortName;     // "Bus" (multiview, compact places)
    Algorithm algorithm;       // engine used for this part
    const char* why;           // why this engine suits this part (tooltip / hint line)
    /** Starting point applied when the part is picked in the UI (parameter id, plain value). */
    std::vector<std::pair<juce::String, float>> startingPoint;
};

const RoleInfo& getRoleInfo (int role);
const AlgorithmInfo& getAlgorithmInfo (int algorithm);
juce::StringArray getRoleNames();
/** "By Drum Part", then every engine. */
juce::StringArray getEngineChoiceNames();

/** The engine that runs for a role and the `engine` parameter (0 = the role's own engine). */
inline Algorithm resolveAlgorithm (int role, int engineChoice) noexcept
{
    if (engineChoice > 0 && engineChoice <= kNumAlgorithms) return (Algorithm) (engineChoice - 1);
    return getRoleInfo (juce::jlimit (0, kNumRoles - 1, role)).algorithm;
}

} // namespace stdc
