#pragma once
#include "../Roles.h"

namespace stdc::style
{

/** ST DC's identity: the shared ST tokens recoloured as dark walnut (surfaces, display haze), a copper accent and
    a red gain-reduction trace (Theme::danger). Called once from the processor constructor. */
void applyTheme();

/** Procedural walnut grain (horizontal, generated once, ~2x the design size). */
const juce::Image& woodTexture();

/** Fills `area` with wood grain (offset so neighbouring areas line up), then darkens it by `shade` (0..1). */
void paintWood (juce::Graphics&, juce::Rectangle<float> area, float shade, juce::Point<float> textureOffset = {});

/** Wooden side cheek (end panel) with a bevel: lit on its inner edge. `leftSide` mirrors the bevel. */
void paintCheek (juce::Graphics&, juce::Rectangle<float> area, bool leftSide);

/** Gain-reduction colour (red, like a VU's GR needle). */
juce::Colour grColour();

/** Small line glyph for a drum part (kick, snare, tom...), stroke style, in `area`. */
void drawRoleGlyph (juce::Graphics&, int role, juce::Rectangle<float> area, juce::Colour, float stroke = 1.4f);

} // namespace stdc::style
