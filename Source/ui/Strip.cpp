#include "Strip.h"

namespace stdc
{

namespace
{
    constexpr int partWidth = 104;
}

DcStrip::DcStrip (StDcProcessor& p) : proc (p), reader (p.getEngine().history, 1024)
{
    const auto& t = st::Theme::get();
    auto make = [&] (const juce::String& id, const char* label)
    {
        auto k = std::make_unique<st::Knob> (proc.param (id), &proc.undoManager, label);
        k->setSize (st::Knob::Size::small);
        k->setLook (st::Knob::Look::arc);
        k->setColour (t.accent);
        addAndMakeVisible (*k);
        knobs.push_back (std::move (k));
    };
    make (ids::threshold, "Thresh");
    make (ids::ratio, "Ratio");
    make (ids::attack, "Attack");
    make (ids::release, "Release");
    make (ids::mix, "Mix");
    make (ids::outputGain, "Out");
    startTimerHz (30);
}

DcStrip::~DcStrip() { stopTimer(); }

int DcStrip::knobWidth (int h) { return juce::jlimit (52, 64, juce::roundToInt ((float) h * 0.38f)); }
int DcStrip::historyWidth (int h) { return juce::jlimit (100, 220, juce::roundToInt ((float) h * 1.1f)); }

int DcStrip::widthFor (int height)
{
    return partWidth + historyWidth (height) + 12 + 6 * knobWidth (height) + 14;
}

void DcStrip::resized()
{
    auto r = getLocalBounds();
    partArea = r.removeFromLeft (partWidth);
    powerArea = juce::Rectangle<int> (partArea.getX() + 4, partArea.getY() + 4, 20, 20);
    historyArea = r.removeFromLeft (historyWidth (getHeight())).reduced (0, 6);
    r.removeFromLeft (12);
    const int kw = knobWidth (getHeight());
    const int kh = juce::jmin (r.getHeight() - 8, kw + 18);
    auto row = r.withSizeKeepingCentre (kw * (int) knobs.size(), kh).withX (r.getX());
    for (auto& k : knobs) k->setBounds (row.removeFromLeft (kw));
}

void DcStrip::timerCallback()
{
    // deepest reduction since the last tick, held then released smoothly (readable at 30 fps)
    const auto peak = reader.peakOfNewest (reader.update());
    gr = peak.grDb < gr ? peak.grDb : gr + 0.25f * (peak.grDb - gr);
    repaint (historyArea.expanded (2));
    repaint (partArea);
}

void DcStrip::paint (juce::Graphics& g)
{
    const auto& t = st::Theme::get();
    g.setColour (t.bg1);
    g.fillRect (getLocalBounds());
    // the part label sits in a soft brown light
    {
        const auto pa = partArea.toFloat();
        g.setGradientFill (juce::ColourGradient (t.displayGlow.withAlpha (0.42f), pa.getCentreX(), pa.getY() + pa.getHeight() * 0.3f,
                                                 t.displayGlow.withAlpha (0.0f), pa.getRight() + 10.0f, pa.getBottom(), true));
        g.fillRect (pa);
    }
    st::paint::fadedLine (g, { (float) partArea.getRight() - 0.5f, 6.0f }, { (float) partArea.getRight() - 0.5f, (float) getHeight() - 6.0f },
                          juce::Colours::white.withAlpha (0.08f), 1.0f, 0.3f);

    // bypass switch
    const bool bypassed = proc.apvts.getRawParameterValue (ids::bypass)->load() > 0.5f;
    st::icons::draw (g, st::icons::Icon::power, powerArea.toFloat().reduced (3.0f), bypassed ? t.warning : t.textDim.withAlpha (0.7f), 1.4f);

    // drum part
    const int role = proc.getRole();
    const auto& info = getRoleInfo (role);
    auto pa = partArea.toFloat().reduced (10.0f, 8.0f);
    const float gs = juce::jlimit (18.0f, 34.0f, (float) getHeight() * 0.22f);
    style::drawRoleGlyph (g, role, juce::Rectangle<float> (gs, gs).withCentre ({ pa.getCentreX(), pa.getY() + 6.0f + gs * 0.5f }), t.accent2, 1.5f);
    auto text = pa.withTrimmedTop (gs + 10.0f);
    g.setFont (st::fonts::semibold (13.0f));
    g.setColour (t.text);
    g.drawText (juce::String (info.name) + juce::String (juce::CharPointer_UTF8 (" \xe2\x96\xbe")), text.removeFromTop (17.0f), juce::Justification::centred, false);
    g.setFont (st::fonts::medium (10.5f));
    g.setColour (t.textDim);
    g.drawText (getAlgorithmInfo ((int) proc.getAlgorithm()).name, text.removeFromTop (14.0f), juce::Justification::centred, false);
    if (text.getHeight() >= 16.0f)
    {
        g.setFont (st::fonts::semibold (12.0f));
        g.setColour (gr < -0.05f ? style::grColour() : t.textFaint);
        g.drawText (st::params::formatDb (gr, 1, true, false), text.removeFromTop (18.0f).translated (0.0f, 2.0f), juce::Justification::centred, false);
    }

    // history: input (dark), output (light), gain reduction (red) and the threshold
    const auto h = historyArea.toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.fillRoundedRectangle (h, 5.0f);
    constexpr float range = 48.0f;
    auto yOf = [&] (float db) { return juce::jmap (juce::jlimit (-range, 0.0f, db), -range, 0.0f, h.getBottom() - 3.0f, h.getY() + 3.0f); };
    const int W = juce::jmax (1, (int) h.getWidth());
    const double fpp = 3.0 * reader.getFrameRate() / (double) W;   // 3 seconds
    juce::Path in, out, grp;
    in.startNewSubPath (h.getRight(), h.getBottom());
    out.startNewSubPath (h.getRight(), h.getBottom());
    double f0 = 0.0;
    float lastX = h.getRight();
    for (int k = 0; k < W; ++k)
    {
        const int a = (int) f0, b = juce::jmax (a + 1, (int) (f0 + fpp));
        if (b > reader.size()) break;
        f0 += fpp;
        float li = -120.0f, lo = -120.0f, lg = 0.0f;
        for (int f = a; f < b; ++f)
        {
            const auto& fr = reader.get (f);
            li = juce::jmax (li, fr.inDb); lo = juce::jmax (lo, fr.outDb); lg = juce::jmin (lg, fr.grDb);
        }
        const float x = h.getRight() - (float) k;
        lastX = x;
        in.lineTo (x, yOf (li));
        out.lineTo (x, yOf (lo));
        const float yg = juce::jmap (juce::jlimit (0.0f, range, -lg), 0.0f, range, h.getY() + 3.0f, h.getBottom() - 3.0f);
        if (k == 0) grp.startNewSubPath (x, yg); else grp.lineTo (x, yg);
    }
    in.lineTo (lastX, h.getBottom()); in.closeSubPath();
    out.lineTo (lastX, h.getBottom()); out.closeSubPath();
    g.saveState();
    g.reduceClipRegion (historyArea);
    g.setColour (t.textFaint.withAlpha (0.3f));
    g.fillPath (in);
    g.setColour (t.textDim.withAlpha (0.28f));
    g.fillPath (out);
    g.setColour (style::grColour());
    g.strokePath (grp, juce::PathStrokeType (1.4f));
    const float ty = yOf (proc.apvts.getRawParameterValue (ids::threshold)->load());
    const float dashes[] = { 3.0f, 3.0f };
    g.setColour (t.accent.withAlpha (0.6f));
    g.drawDashedLine ({ h.getX() + 4.0f, ty, h.getRight() - 4.0f, ty }, dashes, 2, 1.0f);
    g.restoreState();
}

void DcStrip::mouseMove (const juce::MouseEvent& e)
{
    setMouseCursor (partArea.contains (e.getPosition()) ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void DcStrip::mouseDown (const juce::MouseEvent& e)
{
    if (powerArea.expanded (2).contains (e.getPosition()))
    {
        auto& p = proc.param (ids::bypass);
        p.beginChangeGesture();
        p.setValueNotifyingHost (p.getValue() > 0.5f ? 0.0f : 1.0f);
        p.endChangeGesture();
        repaint();
        return;
    }
    if (! partArea.contains (e.getPosition())) return;

    // native menu at the mouse (works for remote views too)
    juce::PopupMenu m;
    m.addSectionHeader ("Drum Part");
    for (int i = 0; i < kNumRoles; ++i)
    {
        const auto& info = getRoleInfo (i);
        m.addItem (juce::PopupMenu::Item (juce::String (info.name) + juce::String (juce::CharPointer_UTF8 ("  \xe2\x80\x94  ")) + getAlgorithmInfo ((int) info.algorithm).name)
                       .setTicked (i == proc.getRole())
                       .setAction ([safe = juce::Component::SafePointer<DcStrip> (this), i] { if (safe != nullptr) safe->proc.selectRole (i); }));
    }
    st::showMenu (m, this, {}, true);
}

} // namespace stdc
