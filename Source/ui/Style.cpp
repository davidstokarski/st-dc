#include "Style.h"

namespace stdc::style
{

void applyTheme()
{
    // Near-black with a warm brown cast: neutral enough to read like a studio tool, brown in the fades and accents.
    auto& t = st::Theme::get();
    t.bg0 = juce::Colour (0xff0a0908);
    t.bg1 = juce::Colour (0xff11100e);
    t.bg2 = juce::Colour (0xff191613);
    t.bg3 = juce::Colour (0xff221e1a);
    t.bg4 = juce::Colour (0xff2e2823);
    t.stroke = juce::Colour (0xff2c2622);
    t.strokeSoft = juce::Colour (0xff1b1815);
    t.highlight = juce::Colour (0x12ffffff);
    t.displayTop = juce::Colour (0xff1d1610);    // dark walnut haze at the top of the window
    t.displayGlow = juce::Colour (0xff3e2b1c);   // warm light (selected part, top glow)
    t.glassPanel = juce::Colour (0xb81c1815);
    t.knobLight = juce::Colour (0xffb9b5b1);
    t.knobDark = juce::Colour (0xff6b6763);
    t.buttonTop = juce::Colour (0xff56514d);
    t.buttonBottom = juce::Colour (0xff3e3a37);
    t.text = juce::Colour (0xffeee9e4);
    t.textDim = juce::Colour (0xffa9a098);
    t.textFaint = juce::Colour (0xff6d655e);
    t.accent = juce::Colour (0xffc4834d);        // caramel / copper
    t.accent2 = juce::Colour (0xffe3b483);       // warm sand
    t.spectrum = juce::Colour (0xffc8b8a8);
    t.curve = juce::Colour (0xfff0dfc8);
    t.danger = juce::Colour (0xffe2513f);        // gain reduction
    t.warning = juce::Colour (0xffe8c15a);
}

juce::Colour grColour() { return st::Theme::get().danger; }

void paintBackdrop (juce::Graphics& g, juce::Rectangle<float> r)
{
    const auto& t = st::Theme::get();
    // base: dark brown at the top fading to near-black
    juce::ColourGradient base (t.displayTop, r.getCentreX(), r.getY(), t.bg0, r.getCentreX(), r.getBottom(), false);
    base.addColour (0.42, t.displayTop.interpolatedWith (t.bg0, 0.55f));
    base.addColour (0.78, t.bg1.interpolatedWith (t.bg0, 0.5f));
    g.setGradientFill (base);
    g.fillRect (r);

    // brown fades: a wide warm light over the top centre, softer pools low left and right
    auto pool = [&] (juce::Point<float> c, float radius, juce::Colour col, float alpha)
    {
        juce::ColourGradient rg (col.withAlpha (alpha), c.x, c.y, col.withAlpha (0.0f), c.x + radius, c.y, true);
        g.setGradientFill (rg);
        g.fillRect (r);
    };
    pool ({ r.getCentreX(), r.getY() - r.getHeight() * 0.1f }, r.getWidth() * 0.62f, t.displayGlow, 0.38f);
    pool ({ r.getX() + r.getWidth() * 0.08f, r.getY() + r.getHeight() * 0.72f }, r.getWidth() * 0.34f, t.displayGlow, 0.14f);
    pool ({ r.getRight() - r.getWidth() * 0.06f, r.getY() + r.getHeight() * 0.40f }, r.getWidth() * 0.30f, t.displayGlow, 0.12f);

    // vignette towards the corners
    juce::ColourGradient vignette (juce::Colours::transparentBlack, r.getCentreX(), r.getY() + r.getHeight() * 0.42f,
                                   juce::Colours::black.withAlpha (0.42f), r.getX() - r.getWidth() * 0.05f, r.getY() + r.getHeight() * 0.42f, true);
    g.setGradientFill (vignette);
    g.fillRect (r);
}

void paintTray (juce::Graphics& g, juce::Rectangle<float> r)
{
    const auto& t = st::Theme::get();
    juce::Path bar;
    bar.addRoundedRectangle (r.getX() + 6.0f, r.getY(), r.getWidth() - 12.0f, r.getHeight() + 20.0f, 16.0f, 16.0f, true, true, false, false);
    g.setGradientFill (juce::ColourGradient (t.bg2.withAlpha (0.92f), 0.0f, r.getY(), t.bg0.withAlpha (0.96f), 0.0f, r.getBottom(), false));
    g.fillPath (bar);
    // a faint brown fade in the middle of the tray
    g.saveState();
    g.reduceClipRegion (bar);
    g.setGradientFill (juce::ColourGradient (t.displayGlow.withAlpha (0.16f), r.getCentreX(), r.getY(),
                                             t.displayGlow.withAlpha (0.0f), r.getCentreX() + r.getWidth() * 0.45f, r.getY(), true));
    g.fillRect (r);
    g.restoreState();
    st::paint::fadedLine (g, { r.getX() + 24.0f, r.getY() + 0.5f }, { r.getRight() - 24.0f, r.getY() + 0.5f },
                          juce::Colours::white.withAlpha (0.10f), 1.0f, 0.18f);
}

//==============================================================================
void drawRoleGlyph (juce::Graphics& g, int role, juce::Rectangle<float> a, juce::Colour c, float stroke)
{
    const auto r = a.withSizeKeepingCentre (juce::jmin (a.getWidth(), a.getHeight()), juce::jmin (a.getWidth(), a.getHeight()));
    const float s = r.getWidth();
    const float x0 = r.getX(), y0 = r.getY();
    auto P = [&] (float fx, float fy) { return juce::Point<float> (x0 + fx * s, y0 + fy * s); };
    juce::Path p;
    g.setColour (c);
    const juce::PathStrokeType st (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    switch ((Role) role)
    {
        case Role::bus:   // three stacked faders merging: a bus
        {
            for (int i = 0; i < 3; ++i)
            {
                const float x = 0.22f + 0.28f * (float) i;
                p.startNewSubPath (P (x, 0.15f)); p.lineTo (P (x, 0.85f));
                const float y = 0.35f + 0.18f * (float) ((i * 2) % 3);
                g.fillRoundedRectangle (juce::Rectangle<float> (s * 0.18f, s * 0.08f).withCentre (P (x, y)), 1.0f);
            }
            break;
        }
        case Role::kick:   // kick drum from the front: shell + head + beater
        {
            p.addEllipse (juce::Rectangle<float> (s * 0.72f, s * 0.72f).withCentre (P (0.5f, 0.52f)));
            p.addEllipse (juce::Rectangle<float> (s * 0.34f, s * 0.34f).withCentre (P (0.5f, 0.52f)));
            p.startNewSubPath (P (0.5f, 0.06f)); p.lineTo (P (0.5f, 0.3f));
            break;
        }
        case Role::snare:   // snare from the side: short cylinder with sticks crossing
        {
            p.addEllipse (juce::Rectangle<float> (s * 0.76f, s * 0.2f).withCentre (P (0.5f, 0.45f)));
            p.startNewSubPath (P (0.12f, 0.45f)); p.lineTo (P (0.12f, 0.7f));
            p.startNewSubPath (P (0.88f, 0.45f)); p.lineTo (P (0.88f, 0.7f));
            p.addCentredArc (x0 + 0.5f * s, y0 + 0.7f * s, 0.38f * s, 0.1f * s, 0.0f, juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi * 3.0f, true);
            p.startNewSubPath (P (0.2f, 0.08f)); p.lineTo (P (0.46f, 0.36f));
            p.startNewSubPath (P (0.8f, 0.08f)); p.lineTo (P (0.54f, 0.36f));
            break;
        }
        case Role::tom:   // deeper drum, tilted
        {
            juce::Path t;
            t.addEllipse (juce::Rectangle<float> (s * 0.66f, s * 0.2f).withCentre (P (0.5f, 0.3f)));
            t.startNewSubPath (P (0.17f, 0.3f)); t.lineTo (P (0.17f, 0.74f));
            t.startNewSubPath (P (0.83f, 0.3f)); t.lineTo (P (0.83f, 0.74f));
            t.addCentredArc (x0 + 0.5f * s, y0 + 0.74f * s, 0.33f * s, 0.1f * s, 0.0f, juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi * 3.0f, true);
            t.applyTransform (juce::AffineTransform::rotation (-0.18f, x0 + 0.5f * s, y0 + 0.5f * s));
            p = t;
            break;
        }
        case Role::cymbals:   // cymbal on a stand
        {
            p.startNewSubPath (P (0.08f, 0.4f));
            p.quadraticTo (P (0.5f, 0.18f), P (0.92f, 0.4f));
            p.quadraticTo (P (0.5f, 0.3f), P (0.08f, 0.4f));
            p.startNewSubPath (P (0.5f, 0.27f)); p.lineTo (P (0.5f, 0.9f));
            p.startNewSubPath (P (0.32f, 0.9f)); p.lineTo (P (0.68f, 0.9f));
            break;
        }
        case Role::room:   // a room corner with a mic in it
        {
            p.startNewSubPath (P (0.1f, 0.2f)); p.lineTo (P (0.35f, 0.32f)); p.lineTo (P (0.35f, 0.88f));
            p.startNewSubPath (P (0.35f, 0.32f)); p.lineTo (P (0.9f, 0.2f));
            p.addEllipse (juce::Rectangle<float> (s * 0.16f, s * 0.22f).withCentre (P (0.64f, 0.52f)));
            p.startNewSubPath (P (0.64f, 0.63f)); p.lineTo (P (0.64f, 0.86f));
            break;
        }
        case Role::verb:   // decaying echoes
        {
            for (int i = 0; i < 3; ++i)
            {
                const float rad = (0.16f + 0.15f * (float) i) * s;
                p.addCentredArc (x0 + 0.24f * s, y0 + 0.5f * s, rad, rad, 0.0f, 0.45f, juce::MathConstants<float>::pi - 0.45f, true);
            }
            g.fillEllipse (juce::Rectangle<float> (s * 0.12f, s * 0.12f).withCentre (P (0.24f, 0.5f)));
            break;
        }
    }
    g.strokePath (p, st);
}

} // namespace stdc::style
