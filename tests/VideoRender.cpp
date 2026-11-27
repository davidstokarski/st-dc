// Use-case videos of the real editor (website), on the shell's video kit (st_shell/testing/st_video.h): a
// synthesized mix plays through the processor while a scripted pointer works the UI.
//   StDcPluginTests --video <dir> [scene ...]
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <st_shell/testing/st_video.h>

using st::video::Director;
using st::video::Music;
using st::video::Scene;

namespace
{
/** A drum part's row in the part selector (0 Drum Bus, 1 Kick, 2 Snare, 3 Tom, 4 Cymbals, 5 Room, 6 Verb). */
juce::Point<float> partRow (Director& d, int part)
{
    auto* sel = d.findType<stdc::RoleSelector>();
    if (sel == nullptr) return d.ptr.pos;
    const float y = (float) stdc::RoleSelector::headerHeight + ((float) part + 0.5f) * (float) stdc::RoleSelector::rowHeight;
    return d.pointIn (*sel, { (float) sel->getWidth() * 0.4f, y });
}

std::vector<Scene> scenes()
{
    std::vector<Scene> v;

    // 1. Snare punch: pick the part, pull the threshold down, open the attack so the crack comes through
    v.push_back ({ "snare-punch", 15.0, [] (juce::AudioProcessor&, Music& m)
    {
        m.drums = 1.0f; m.rooms = 0.3f;
    }, [] (Director& dir)
    {
        dir.wait (1.3);
        dir.move ([&dir] { return partRow (dir, 2); }, 1.0);
        dir.click();                                                  // Snare: FET engine and a snare starting point
        dir.wait (0.8);
        dir.move ([&dir] { return dir.controlCentre (stdc::ids::threshold); }, 1.0);
        dir.dragBy ({ 0.0f, 55.0f }, 1.4);                            // threshold down: more gain reduction
        dir.wait (0.6);
        dir.move ([&dir] { return dir.controlCentre (stdc::ids::attack); }, 0.9);
        dir.dragBy ({ 0.0f, -45.0f }, 1.1);                           // slower attack: the transient gets through
        dir.move ([&dir] { return dir.controlCentre (stdc::ids::release); }, 0.8);
        dir.dragBy ({ 0.0f, 30.0f }, 0.9);                            // quicker release: snappier
        dir.wait (3.0);
    } });

    // 2. Room mics, smashed and blended back in
    v.push_back ({ "room-crush", 15.0, [] (juce::AudioProcessor&, Music& m)
    {
        m.drums = 0.9f; m.rooms = 1.0f; m.bass = 0.25f;
    }, [] (Director& dir)
    {
        dir.wait (1.3);
        dir.move ([&dir] { return partRow (dir, 5); }, 1.0);
        dir.click();                                                  // Room: the all-buttons FET
        dir.wait (0.8);
        dir.move ([&dir] { return dir.controlCentre (stdc::ids::threshold); }, 1.0);
        dir.dragBy ({ 0.0f, 70.0f }, 1.4);                            // crush it
        dir.move ([&dir] { return dir.controlCentre (stdc::ids::drive); }, 0.9);
        dir.dragBy ({ 0.0f, -60.0f }, 1.1);                           // add drive
        dir.move ([&dir] { return dir.controlCentre (stdc::ids::mix); }, 0.9);
        dir.dragBy ({ 0.0f, 55.0f }, 1.2);                            // blend it back under the dry kit
        dir.wait (2.8);
    } });

    // 3. The website reel's ST DC segment: the Snare part compressing the kit, no pointer (it waits off screen).
    //    The reel takes 4.3 s of it (from 1.5 s) between the ST Sat and ST Split segments, 0.5 s cross-fades.
    v.push_back ({ "reel", 7.0, [] (juce::AudioProcessor& p, Music& m)
    {
        m.drums = 1.0f; m.rooms = 0.3f;
        if (auto* dc = dynamic_cast<StDcProcessor*> (&p)) dc->selectRole ((int) stdc::Role::snare);
    }, [] (Director& dir)
    {
        dir.move ([] { return juce::Point<float> (-200.0f, -200.0f); }, 0.01);
        dir.wait (7.0);
    } });
    return v;
}
}

int runVideoRender (int argc, char** argv);
int runVideoRender (int argc, char** argv)
{
    return st::video::run (argc, argv, "st-dc", [] { return std::make_unique<StDcProcessor>(); }, scenes());
}
