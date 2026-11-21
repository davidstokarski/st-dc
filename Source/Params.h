#pragma once
#include <st_shell/st_shell.h>

namespace stdc
{

/** Parameter IDs. Permanent: never rename them after a release (sessions and presets depend on them). */
namespace ids
{
    inline const juce::String role          = "role";           // drum part (selects the engine)
    inline const juce::String engine        = "engine";         // 0 = by drum part, else an explicit engine
    inline const juce::String inputGain     = "input_gain";
    inline const juce::String threshold     = "threshold";
    inline const juce::String ratio         = "ratio";
    inline const juce::String knee          = "knee";
    inline const juce::String range         = "range";
    inline const juce::String attack        = "attack";
    inline const juce::String release       = "release";
    inline const juce::String autoRelease   = "auto_release";
    inline const juce::String drive         = "drive";          // saturation amount
    inline const juce::String character     = "character";      // saturation flavour
    inline const juce::String mix           = "mix";            // parallel (dry/wet) compression
    inline const juce::String outputGain    = "output_gain";
    inline const juce::String autoGain      = "auto_gain";
    inline const juce::String scHighPass    = "sc_hpf";         // side-chain high-pass
    inline const juce::String stereoLink    = "stereo_link";
    inline const juce::String scExternal    = "sc_external";    // detect on the side-chain input bus
    inline const juce::String quality       = "oversampling";
    inline const juce::String bypass        = "bypass";

    /** Parameters a drum part sets when it is picked in the UI (its starting point). */
    const juce::StringArray& roleSettingIds();
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

} // namespace stdc
