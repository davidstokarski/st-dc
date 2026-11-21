#include "FactoryPresets.h"
#include "Roles.h"

namespace stdc
{

namespace
{
    using Values = std::vector<std::pair<juce::String, float>>;

    /** A drum part's starting point, then overrides. */
    Values part (Role r, std::initializer_list<std::pair<juce::String, float>> overrides = {})
    {
        Values v { { ids::role, (float) (int) r }, { ids::engine, 0.0f } };
        for (auto& p : getRoleInfo ((int) r).startingPoint) v.push_back (p);
        for (auto& p : overrides) v.push_back (p);
        return v;
    }
}

void registerFactoryPresets (st::PresetManager& pm)
{
    using namespace ids;
    auto add = [&pm] (const char* name, const char* category, const Values& v) { pm.addFactoryPreset (name, category, v); };

    // one starting point per drum part (the first one is what a new instance sounds like)
    add ("Init", "Basics", {});
    add ("Drum Bus Glue", "Drum Parts", part (Role::bus));
    add ("Kick Punch", "Drum Parts", part (Role::kick));
    add ("Snare Crack", "Drum Parts", part (Role::snare));
    add ("Tom Sustain", "Drum Parts", part (Role::tom));
    add ("Cymbal Leveller", "Drum Parts", part (Role::cymbals));
    add ("Room Crush", "Drum Parts", part (Role::room));
    add ("Verb Density", "Drum Parts", part (Role::verb));

    // variations
    add ("Parallel Bus Smash", "Drum Bus", part (Role::bus, { { threshold, -30.0f }, { ratio, 10.0f }, { attack, 1.0f },
                                                             { release, 80.0f }, { autoRelease, 0.0f }, { range, 30.0f },
                                                             { drive, 30.0f }, { character, 0.0f }, { mix, 35.0f } }));
    add ("Gentle Bus 2:1", "Drum Bus", part (Role::bus, { { threshold, -14.0f }, { ratio, 2.0f }, { attack, 30.0f }, { range, 6.0f } }));
    add ("Kick Thump", "Kick", part (Role::kick, { { attack, 25.0f }, { release, 140.0f }, { ratio, 6.0f }, { drive, 30.0f } }));
    add ("Kick Tight", "Kick", part (Role::kick, { { attack, 4.0f }, { release, 60.0f }, { threshold, -24.0f } }));
    add ("Snare Fat", "Snare", part (Role::snare, { { attack, 6.0f }, { release, 150.0f }, { ratio, 8.0f }, { drive, 25.0f } }));
    add ("Snare Parallel", "Snare", part (Role::snare, { { threshold, -34.0f }, { ratio, 20.0f }, { attack, 0.05f }, { mix, 40.0f } }));
    add ("Overheads Smooth", "Cymbals", part (Role::cymbals, { { threshold, -18.0f }, { range, 6.0f }, { character, 1.0f }, { drive, 8.0f } }));
    add ("Room Blend", "Room", part (Role::room, { { mix, 45.0f }, { release, 120.0f } }));
    add ("Verb Ducker (Side-Chain)", "Verb", part (Role::verb, { { scExternal, 1.0f }, { threshold, -30.0f }, { ratio, 6.0f },
                                                                  { attack, 5.0f }, { release, 250.0f }, { autoRelease, 0.0f },
                                                                  { scHighPass, 20.0f }, { range, 12.0f } }));
}

} // namespace stdc
