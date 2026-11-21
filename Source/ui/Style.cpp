#include "Style.h"

namespace stdc::style
{

void applyTheme()
{
    auto& t = st::Theme::get();
    t.bg0 = juce::Colour (0xff0d0907);
    t.bg1 = juce::Colour (0xff16100c);
    t.bg2 = juce::Colour (0xff1f1712);
    t.bg3 = juce::Colour (0xff2b2119);
    t.bg4 = juce::Colour (0xff3b2e24);
    t.stroke = juce::Colour (0xff3b2f25);
    t.strokeSoft = juce::Colour (0xff241b15);
    t.displayTop = juce::Colour (0xff2c1d13);
    t.displayGlow = juce::Colour (0xff6a4122);
    t.glassPanel = juce::Colour (0xb02a2019);
    t.knobLight = juce::Colour (0xffbfb8b0);
    t.knobDark = juce::Colour (0xff6f675f);
    t.buttonTop = juce::Colour (0xff5f5751);
    t.buttonBottom = juce::Colour (0xff453e38);
    t.text = juce::Colour (0xfff1e9e0);
    t.textDim = juce::Colour (0xffb9a999);
    t.textFaint = juce::Colour (0xff7d6c5d);
    t.accent = juce::Colour (0xffd2772b);     // copper
    t.accent2 = juce::Colour (0xffffa851);    // warm amber
    t.spectrum = juce::Colour (0xffdcb894);
    t.curve = juce::Colour (0xfff4dcae);
    t.danger = juce::Colour (0xffec4b3c);
}

juce::Colour grColour() { return st::Theme::get().danger; }

//==============================================================================
namespace
{
    inline uint32_t hash (int x, int y) noexcept
    {
        uint32_t h = (uint32_t) x * 374761393u + (uint32_t) y * 668265263u;
        h = (h ^ (h >> 13)) * 1274126177u;
        return h ^ (h >> 16);
    }
    inline float lattice (int x, int y) noexcept { return (float) (hash (x, y) & 0xffff) / 65535.0f; }

    /** Smooth value noise, 0..1. */
    float noise (float x, float y) noexcept
    {
        const int xi = (int) std::floor (x), yi = (int) std::floor (y);
        float fx = x - (float) xi, fy = y - (float) yi;
        fx = fx * fx * (3.0f - 2.0f * fx);
        fy = fy * fy * (3.0f - 2.0f * fy);
        const float a = lattice (xi, yi), b = lattice (xi + 1, yi), c = lattice (xi, yi + 1), d = lattice (xi + 1, yi + 1);
        return a + (b - a) * fx + (c - a) * fy + (a - b - c + d) * fx * fy;
    }

    juce::Image makeWood()
    {
        constexpr int W = 2200, H = 1440;   // texture pixels; drawn at 2 px per design px
        juce::Image img (juce::Image::RGB, W, H, false);
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);

        // walnut: dark heartwood, lighter late-wood streaks, a little red in the mids
        const juce::Colour dark (0xff160d07), mid (0xff3a2414), light (0xff6a4426);
        for (int y = 0; y < H; ++y)
        {
            const float v = (float) y * 0.5f;
            for (int x = 0; x < W; ++x)
            {
                const float u = (float) x * 0.5f;
                // growth rings: horizontal bands warped by low-frequency noise (figure), stretched along x
                const float warp = 22.0f * noise (u * 0.0035f, v * 0.018f) + 7.0f * noise (u * 0.012f + 31.0f, v * 0.06f);
                const float ring = std::sin ((v + warp) * 0.21f);
                const float band = std::pow (std::abs (ring), 0.55f);
                // pores / fine streaks: long in x, thin in y
                const float fine = noise (u * 0.018f + 7.0f, v * 0.9f);
                const float streak = noise (u * 0.004f, v * 0.25f + 13.0f);
                const float broad = noise (u * 0.0016f + 3.0f, v * 0.003f);
                float tt = 0.42f * band + 0.26f * fine + 0.18f * streak + 0.14f * broad;
                tt = juce::jlimit (0.0f, 1.0f, (tt - 0.18f) * 1.35f);
                const auto c = tt < 0.5f ? dark.interpolatedWith (mid, tt * 2.0f) : mid.interpolatedWith (light, (tt - 0.5f) * 2.0f);
                bd.setPixelColour (x, y, c);
            }
        }
        return img;
    }
}

const juce::Image& woodTexture()
{
    static const juce::Image img = makeWood();
    return img;
}

void paintWood (juce::Graphics& g, juce::Rectangle<float> area, float shade, juce::Point<float> offset)
{
    const auto& img = woodTexture();
    g.saveState();
    g.reduceClipRegion (area.toNearestIntEdges());
    // 2 texture pixels per design pixel; the texture is aligned to the editor (offset = area's position in it)
    const auto src = juce::Rectangle<float> ((area.getX() + offset.x) * 2.0f, (area.getY() + offset.y) * 2.0f,
                                             area.getWidth() * 2.0f, area.getHeight() * 2.0f)
                         .getIntersection (img.getBounds().toFloat());
    g.drawImage (img, juce::roundToInt (area.getX()), juce::roundToInt (area.getY()), juce::roundToInt (area.getWidth()), juce::roundToInt (area.getHeight()),
                 (int) src.getX(), (int) src.getY(), juce::jmax (1, (int) src.getWidth()), juce::jmax (1, (int) src.getHeight()));
    if (shade > 0.0f)
    {
        g.setColour (juce::Colours::black.withAlpha (juce::jlimit (0.0f, 1.0f, shade)));
        g.fillRect (area);
    }
    g.restoreState();
}

void paintCheek (juce::Graphics& g, juce::Rectangle<float> r, bool leftSide)
{
    paintWood (g, r, 0.18f);
    // oil sheen: lighter in the upper middle, darker to the bottom
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.05f), r.getCentreX(), r.getY() + r.getHeight() * 0.25f,
                                             juce::Colours::black.withAlpha (0.45f), r.getCentreX(), r.getBottom(), false));
    g.fillRect (r);
    // bevels: lit inner edge, dark outer edge, both fading at the ends
    const float inner = leftSide ? r.getRight() - 1.0f : r.getX();
    const float outer = leftSide ? r.getX() : r.getRight() - 1.0f;
    st::paint::fadedLine (g, { inner + 0.5f, r.getY() }, { inner + 0.5f, r.getBottom() }, juce::Colours::white.withAlpha (0.14f), 1.0f, 0.08f);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRect (juce::Rectangle<float> (outer, r.getY(), 1.0f, r.getHeight()));
    // shadow cast onto the display
    const float sx = leftSide ? r.getRight() : r.getX() - 10.0f;
    juce::ColourGradient shadow (juce::Colours::black.withAlpha (0.35f), leftSide ? r.getRight() : r.getX(), 0.0f,
                                 juce::Colours::transparentBlack, leftSide ? r.getRight() + 10.0f : r.getX() - 10.0f, 0.0f, false);
    g.setGradientFill (shadow);
    g.fillRect (juce::Rectangle<float> (sx, r.getY(), 10.0f, r.getHeight()));
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
