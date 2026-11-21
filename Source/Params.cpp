#include "Params.h"
#include "Roles.h"

namespace stdc
{

using namespace st::params;

const juce::StringArray& ids::roleSettingIds()
{
    static const juce::StringArray a { threshold, ratio, knee, range, attack, release, autoRelease, drive, character,
                                       mix, scHighPass, stereoLink, autoGain };
    return a;
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto hpfAttributes = juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float v, int) { return v <= 20.5f ? juce::String ("Off") : formatHz (v); })
        .withValueFromStringFunction ([] (const juce::String& s) { return s.containsIgnoreCase ("off") ? 20.0f : parseHz (s); })
        .withLabel ("Hz");

    auto rangeAttributes = juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float v, int) { return v >= 47.5f ? juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x9e")) : formatDb (v, 1, true, false); })
        .withValueFromStringFunction ([] (const juce::String& s) { return s.containsIgnoreCase ("inf") ? 48.0f : std::abs (parseDb (s)); })
        .withLabel ("dB");

    {
        auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("part", "Drum Part", " | ");
        g->addChild (makeChoice (ids::role, "Drum Part", getRoleNames(), 0));
        g->addChild (makeChoice (ids::engine, "Engine", getEngineChoiceNames(), 0));
        layout.add (std::move (g));
    }
    {
        auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("comp", "Compressor", " | ");
        g->addChild (makeFloat (ids::inputGain, "Input Gain", linear (-24.0f, 24.0f, 0.01f), 0.0f, dbAttributes()));
        g->addChild (makeFloat (ids::threshold, "Threshold", linear (-60.0f, 0.0f, 0.01f), -18.0f, dbAttributes()));
        g->addChild (makeFloat (ids::ratio, "Ratio", skewed (1.0f, 100.0f, 6.0f, 0.01f), 4.0f, ratioAttributes()));
        g->addChild (makeFloat (ids::knee, "Knee", linear (0.0f, 24.0f, 0.01f), 6.0f, dbAttributes()));
        g->addChild (makeFloat (ids::range, "Range", linear (0.0f, 48.0f, 0.01f), 24.0f, rangeAttributes));
        g->addChild (makeFloat (ids::attack, "Attack", skewed (0.02f, 250.0f, 5.0f, 0.001f), 10.0f, msAttributes()));
        g->addChild (makeFloat (ids::release, "Release", skewed (5.0f, 2500.0f, 150.0f, 0.01f), 150.0f, msAttributes()));
        g->addChild (makeBool (ids::autoRelease, "Auto Release", false));
        g->addChild (makeFloat (ids::scHighPass, "Side-Chain High-Pass", logarithmic (20.0f, 500.0f), 20.0f, hpfAttributes));
        g->addChild (makeFloat (ids::stereoLink, "Stereo Link", linear (0.0f, 100.0f, 0.01f), 100.0f, percentAttributes()));
        g->addChild (makeBool (ids::scExternal, "External Side-Chain", false));
        layout.add (std::move (g));
    }
    {
        auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("colour", "Saturation", " | ");
        g->addChild (makeFloat (ids::drive, "Drive", linear (0.0f, 100.0f, 0.01f), 0.0f, percentAttributes()));
        g->addChild (makeChoice (ids::character, "Character", { "Tube", "Tape", "Transformer" }, 0));
        layout.add (std::move (g));
    }
    {
        auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("output", "Output", " | ");
        g->addChild (makeFloat (ids::mix, "Mix", linear (0.0f, 100.0f, 0.01f), 100.0f, percentAttributes()));
        g->addChild (makeFloat (ids::outputGain, "Output Gain", linear (-24.0f, 24.0f, 0.01f), 0.0f, dbAttributes()));
        g->addChild (makeBool (ids::autoGain, "Auto Gain", false));
        g->addChild (makeChoice (ids::quality, "Oversampling", { "Off", "2x", "4x" }, 0));
        g->addChild (makeBool (ids::bypass, "Bypass", false));
        layout.add (std::move (g));
    }
    return layout;
}

} // namespace stdc
