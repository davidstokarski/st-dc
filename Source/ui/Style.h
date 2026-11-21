#pragma once
#include "../Roles.h"

namespace stdc::style
{

/** ST DC's identity: the shared ST tokens recoloured near-black with a warm brown cast (display haze, fades), a
    caramel accent and a red gain-reduction trace (Theme::danger). Called once from the processor constructor. */
void applyTheme();

/** The window backdrop: dark brown haze at the top fading to near-black, soft brown light pools, vignette.
    No texture. */
void paintBackdrop (juce::Graphics&, juce::Rectangle<float> area);

/** Footer tray: dark, rounded top corners, faint brown fade and a faded top edge light. */
void paintTray (juce::Graphics&, juce::Rectangle<float> area);

/** Gain-reduction colour (red, like a VU's GR needle). */
juce::Colour grColour();

/** Small line glyph for a drum part (kick, snare, tom...), stroke style, in `area`. */
void drawRoleGlyph (juce::Graphics&, int role, juce::Rectangle<float> area, juce::Colour, float stroke = 1.4f);

} // namespace stdc::style
