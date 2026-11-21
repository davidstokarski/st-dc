#include "Panels.h"

namespace stdc
{

//==============================================================================
RoleSelector::RoleSelector (StDcProcessor& p) : proc (p)
{
    setTooltip ("Drum part: picks the compression engine built for it and loads a starting point (undo with Cmd-Z)");
    lit.snap ((float) proc.getRole());
    startTimerHz (60);
}

juce::Rectangle<float> RoleSelector::rowBounds (int i) const
{
    return { 0.0f, (float) (headerHeight + i * rowHeight), (float) getWidth(), (float) rowHeight - 4.0f };
}

int RoleSelector::rowAt (juce::Point<float> p) const
{
    for (int i = 0; i < kNumRoles; ++i)
        if (rowBounds (i).contains (p)) return i;
    return -1;
}

void RoleSelector::timerCallback()
{
    const int r = proc.getRole(), e = proc.apvts.getRawParameterValue (ids::engine)->load() > 0.5f ? (int) proc.getAlgorithm() : -1;
    bool dirty = r != role || e != engine;
    role = r;
    engine = e;
    lit.setTarget ((float) role);
    dirty |= lit.tick (1.0 / 60.0, 16.0f);
    if (dirty) repaint();
}

void RoleSelector::paint (juce::Graphics& g)
{
    const auto& t = st::Theme::get();
    st::paint::caption (g, "Drum Part", { 10.0f, 0.0f, (float) getWidth() - 10.0f, (float) headerHeight - 4.0f }, t.textFaint,
                        juce::Justification::centredLeft, 9.5f);

    // the light behind the selected row (glides)
    {
        const auto r = rowBounds (0).translated (0.0f, lit.current * (float) rowHeight);
        juce::ColourGradient grad (t.displayGlow.withAlpha (0.55f), r.getX(), r.getCentreY(), t.displayGlow.withAlpha (0.0f), r.getRight(), r.getCentreY(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (t.accent);
        g.fillRoundedRectangle (juce::Rectangle<float> (2.5f, r.getHeight() - 16.0f).withCentre ({ r.getX() + 2.0f, r.getCentreY() }), 1.2f);
    }

    for (int i = 0; i < kNumRoles; ++i)
    {
        const auto& info = getRoleInfo (i);
        const auto r = rowBounds (i);
        const bool sel = i == role, over = i == hover;
        const auto glyphCol = sel ? t.accent2 : (over ? t.text : t.textDim.withAlpha (0.85f));
        style::drawRoleGlyph (g, i, juce::Rectangle<float> (24.0f, 24.0f).withCentre ({ r.getX() + 22.0f, r.getCentreY() }), glyphCol, sel ? 1.6f : 1.3f);

        auto text = r.withTrimmedLeft (42.0f);
        g.setFont (st::fonts::semibold (13.0f));
        g.setColour (sel ? t.text : (over ? t.text.withAlpha (0.9f) : t.textDim));
        g.drawText (info.name, text.removeFromTop (r.getHeight() * 0.56f).withTrimmedTop (3.0f), juce::Justification::bottomLeft, false);

        // the engine this part runs (the override engine, in amber, for the selected part when it's set)
        const bool overridden = sel && engine >= 0;
        const juce::String eng = overridden ? juce::String (getAlgorithmInfo (engine).name) + juce::String (juce::CharPointer_UTF8 (" \xe2\x80\xa2"))
                                            : juce::String (getAlgorithmInfo ((int) info.algorithm).name);
        g.setFont (st::fonts::medium (10.5f));
        g.setColour (overridden ? t.accent2 : (sel ? t.textDim : t.textFaint));
        g.drawText (eng, text.withTrimmedTop (1.0f), juce::Justification::topLeft, false);
    }
}

void RoleSelector::mouseMove (const juce::MouseEvent& e)
{
    const int h = rowAt (e.position);
    if (h != hover) { hover = h; repaint(); }
    setMouseCursor (h >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void RoleSelector::mouseDown (const juce::MouseEvent& e)
{
    const int r = rowAt (e.position);
    if (r < 0) return;
    if (e.mods.isPopupMenu())
    {
        // right-click: switch the engine only, keep the settings
        juce::PopupMenu m;
        m.addSectionHeader (getRoleInfo (r).name);
        m.addItem (juce::PopupMenu::Item ("Select and load starting point").setAction ([this, r] { proc.selectRole (r); }));
        m.addItem (juce::PopupMenu::Item ("Select, keep current settings").setAction ([this, r]
        {
            auto& p = proc.param (ids::role);
            proc.undoManager.beginNewTransaction ("Drum part");
            p.beginChangeGesture();
            p.setValueNotifyingHost (p.convertTo0to1 ((float) r));
            p.endChangeGesture();
        }));
        st::showMenu (m, this, {}, true);
        return;
    }
    proc.selectRole (r);
}

//==============================================================================
ControlBar::ControlBar (StDcProcessor& p) : proc (p)
{
    const auto& t = st::Theme::get();
    auto* um = &proc.undoManager;

    auto knob = [&] (std::unique_ptr<st::Knob>& k, const juce::String& id, const char* label, st::Knob::Size size,
                     st::Knob::Look look, const char* tip)
    {
        k = std::make_unique<st::Knob> (proc.param (id), um, label);
        k->setSize (size);
        k->setLook (look);
        k->setColour (t.accent);
        k->setTooltip (tip);
        addAndMakeVisible (*k);
    };
    using S = st::Knob::Size;
    using L = st::Knob::Look;
    knob (input, ids::inputGain, "Input", S::medium, L::classic, "Input gain into the compressor (the Mix dry signal gets it too)");
    knob (threshold, ids::threshold, "Threshold", S::large, L::ringed, "Level where compression starts (also draggable in the display)");
    knob (ratio, ids::ratio, "Ratio", S::medium, L::classic, "How hard levels above the threshold are reduced");
    knob (knee, ids::knee, "Knee", S::medium, L::classic, "Width of the soft transition around the threshold");
    knob (range, ids::range, "Range", S::medium, L::classic, "Maximum gain reduction");
    knob (attack, ids::attack, "Attack", S::medium, L::classic, "How fast gain reduction starts. Longer lets more of the hit through.");
    knob (release, ids::release, "Release", S::medium, L::classic, "How fast gain reduction recovers");
    knob (drive, ids::drive, "Drive", S::medium, L::ringed, "Saturation after compression (pick the character above)");
    knob (mix, ids::mix, "Mix", S::medium, L::classic, "Parallel compression: blend of dry and compressed signal (latency aligned)");
    knob (output, ids::outputGain, "Output", S::medium, L::ringed, "Output gain");
    knob (link, ids::stereoLink, "Link", S::small, L::classic, "Stereo link: 100% = one gain for both sides");
    knob (hpf, ids::scHighPass, "SC HPF", S::small, L::classic, "Side-chain high-pass: the detector ignores the lows (less kick pumping)");
    threshold->setBipolar (false);

    engine = std::make_unique<st::Dropdown> (proc.param (ids::engine), um);
    engine->setGreyStyle (true);
    engine->setTooltip ("Engine: By Drum Part follows the selected part; or pick any engine for this part");
    engine->buildMenu = [] (int current)
    {
        juce::PopupMenu m;
        m.addItem (juce::PopupMenu::Item ("By Drum Part").setID (1).setTicked (current == 0));
        m.addSectionHeader ("Engine");
        for (int i = 0; i < kNumAlgorithms; ++i)
        {
            const auto& a = getAlgorithmInfo (i);
            m.addItem (juce::PopupMenu::Item (juce::String (a.name) + juce::String (juce::CharPointer_UTF8 ("  \xe2\x80\x94  ")) + a.character)
                           .setID (i + 2).setTicked (current == i + 1));
        }
        return m;
    };
    character = std::make_unique<st::Dropdown> (proc.param (ids::character), um);
    character->setGreyStyle (true);
    character->setTooltip ("Saturation character: Tube (even harmonics), Tape (soft, darker), Transformer (thick low end)");

    autoRelease = std::make_unique<st::ToggleChip> (proc.param (ids::autoRelease), um, "Auto", t.accent2);
    autoRelease->setGreyStyle (true);
    autoRelease->setTooltip ("Programme-dependent release: short hits recover fast, sustained compression slowly");
    autoGain = std::make_unique<st::ToggleChip> (proc.param (ids::autoGain), um, "Auto Gain", t.accent2);
    autoGain->setGreyStyle (true);
    autoGain->setTooltip ("Make-up gain estimated from threshold, ratio, knee and drive");
    external = std::make_unique<st::ToggleChip> (proc.param (ids::scExternal), um, "External", t.accent2);
    external->setGreyStyle (true);
    external->setTooltip ("Detect on the side-chain input (e.g. duck the reverb from the dry drums)");

    for (juce::Component* c : { (juce::Component*) engine.get(), (juce::Component*) character.get(), (juce::Component*) autoRelease.get(),
                                (juce::Component*) autoGain.get(), (juce::Component*) external.get() })
        addAndMakeVisible (c);
}

st::Knob* ControlBar::getKnob (const juce::String& id) const
{
    for (auto* k : { input.get(), threshold.get(), ratio.get(), knee.get(), range.get(), attack.get(), release.get(), drive.get(),
                     mix.get(), output.get(), link.get(), hpf.get() })
        if (k != nullptr && k->getParameter().getParameterID() == id) return k;
    return nullptr;
}

void ControlBar::resized()
{
    const int w = getWidth();
    box = juce::Rectangle<float> (6.0f, 22.0f, (float) w - 12.0f, 64.0f);
    dividers.clear();
    groupCaptions.clear();

    const int ky = 36, kw = 68, kh = 76;
    int x = 18;
    input->setBounds (x, ky, kw, kh);            x += kw + 10;
    dividers.push_back ((float) x - 4.0f);
    threshold->setBounds (x, 2, 112, 132);        x += 112;
    engine->setBounds (x - 112 - 6, 10, 0, 0);
    ratio->setBounds (x, ky, kw, kh);            x += kw;
    knee->setBounds (x, ky, kw, kh);             x += kw;
    range->setBounds (x, ky, kw, kh);            x += kw + 10;
    engine->setBounds (threshold->getRight() + 6, 10, x - threshold->getRight() - 22, 23);
    dividers.push_back ((float) x - 4.0f);
    attack->setBounds (x, ky, kw, kh);           x += kw;
    release->setBounds (x, ky, kw, kh);
    autoRelease->setBounds (release->getBounds().getCentreX() - 26, 10, 52, 23);
    x += kw + 10;
    dividers.push_back ((float) x - 4.0f);
    drive->setBounds (x, 34, 86, 96);
    character->setBounds (drive->getBounds().getCentreX() - 58, 10, 116, 23);
    x += 86;
    mix->setBounds (x, ky, kw, kh);              x += kw;
    output->setBounds (x, 34, 86, 96);
    autoGain->setBounds (output->getBounds().getCentreX() - 40, 10, 80, 23);
    x += 86 + 10;
    dividers.push_back ((float) x - 4.0f);
    link->setBounds (x, 40, 60, 66);             x += 60;
    hpf->setBounds (x, 40, 60, 66);
    external->setBounds (link->getX() + 60 - 38, 10, 76, 23);

    // centre the whole row in the bar
    const int shift = juce::jmax (0, (w - (hpf->getRight() + 18)) / 2);
    for (auto* c : getChildren()) c->setTopLeftPosition (c->getX() + shift, c->getY());
    for (auto& d : dividers) d += (float) shift;
}

void ControlBar::paint (juce::Graphics& g)
{
    // the bar: translucent with a faint edge light (bright in the middle, fading towards both ends)
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (box.translated (0.0f, 3.0f).expanded (2.0f), 9.0f);
    g.setColour (st::Theme::get().glassPanel);
    g.fillRoundedRectangle (box, 8.0f);
    g.setColour (juce::Colours::white.withAlpha (0.035f));
    g.drawRoundedRectangle (box.reduced (0.5f), 8.0f, 1.0f);
    juce::ColourGradient edge (juce::Colours::white.withAlpha (0.0f), box.getX(), 0.0f, juce::Colours::white.withAlpha (0.0f), box.getRight(), 0.0f, false);
    edge.addColour (0.25, juce::Colours::white.withAlpha (0.13f));
    edge.addColour (0.75, juce::Colours::white.withAlpha (0.13f));
    g.setGradientFill (edge);
    g.fillRect (box.getX() + 8.0f, box.getY() + 0.5f, box.getWidth() - 16.0f, 1.0f);

    for (float x : dividers)
        st::paint::fadedLine (g, { x, box.getY() + 8.0f }, { x, box.getBottom() + 34.0f }, juce::Colours::white.withAlpha (0.08f), 1.0f, 0.35f);
}

//==============================================================================
FooterBar::FooterBar (StDcProcessor& p)
    : quality (p.param (ids::quality), &p.undoManager),
      bypass (p.param (ids::bypass), &p.undoManager, "Bypass", st::Theme::get().warning)
{
    quality.setFlatStyle (true);
    quality.setCaption ("Oversampling");
    quality.setTooltip ("Oversampling for the colour stage: less aliasing from Drive / FET distortion, adds latency");
    bypass.setFlatStyle (true);
    bypass.setTooltip ("Bypass (latency compensated)");
    range.setDropdownArrow (true);
    speed.setDropdownArrow (true);
    multi.setDropdownArrow (false);

    range.onClick = [this]
    {
        juce::PopupMenu m;
        for (float r : { 24.0f, 48.0f, 72.0f })
            m.addItem (juce::PopupMenu::Item (juce::String ((int) r) + " dB").setTicked (std::abs (r - rangeDb) < 0.5f)
                           .setAction ([this, r] { setDisplay (r, seconds); if (onDisplayChanged) onDisplayChanged (rangeDb, seconds); }));
        st::showMenu (m, &range);
    };
    speed.onClick = [this]
    {
        juce::PopupMenu m;
        for (float s : { 2.0f, 4.0f, 8.0f, 16.0f })
            m.addItem (juce::PopupMenu::Item (juce::String ((int) s) + " s").setTicked (std::abs (s - seconds) < 0.1f)
                           .setAction ([this, s] { setDisplay (rangeDb, s); if (onDisplayChanged) onDisplayChanged (rangeDb, seconds); }));
        st::showMenu (m, &speed);
    };
    multi.onClick = [this] { if (onMultiView) onMultiView(); };

    for (juce::Component* c : { (juce::Component*) &quality, (juce::Component*) &range, (juce::Component*) &speed,
                                (juce::Component*) &multi, (juce::Component*) &bypass })
        addAndMakeVisible (c);
    refreshText();
}

void FooterBar::setDisplay (float r, float s)
{
    rangeDb = r;
    seconds = s;
    refreshText();
}

void FooterBar::refreshText()
{
    range.setButtonText (juce::String ((int) rangeDb) + " dB");
    range.setDimmedText ("Range  ");
    speed.setButtonText (juce::String ((int) seconds) + " s");
    speed.setDimmedText ("Time  ");
    repaint();
}

void FooterBar::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    // wooden tray: walnut under a dark lacquer, rounded top corners, faded top light
    juce::Path bar;
    bar.addRoundedRectangle (r.getX() + 6.0f, r.getY(), r.getWidth() - 12.0f, r.getHeight() + 20.0f, 18.0f, 18.0f, true, true, false, false);
    g.saveState();
    g.reduceClipRegion (bar);
    style::paintWood (g, r, 0.55f, getPosition().toFloat());
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.03f), 0.0f, r.getY(), juce::Colours::black.withAlpha (0.25f), 0.0f, r.getBottom(), false));
    g.fillRect (r);
    g.restoreState();
    st::paint::fadedLine (g, { r.getX() + 24.0f, r.getY() + 0.5f }, { r.getRight() - 24.0f, r.getY() + 0.5f }, juce::Colours::white.withAlpha (0.12f), 1.0f, 0.15f);

    for (float x : dividers)
        st::paint::fadedLine (g, { x + 0.5f, r.getY() + 8.0f }, { x + 0.5f, r.getBottom() - 8.0f }, juce::Colours::white.withAlpha (0.10f), 1.0f, 0.5f);
}

void FooterBar::resized()
{
    auto r = getLocalBounds().reduced (28, 0);
    dividers.clear();
    auto divide = [this, &r] { dividers.push_back ((float) r.getX() + 10.0f); r.removeFromLeft (20); };
    quality.setBounds (r.removeFromLeft (150));
    divide();
    range.setBounds (r.removeFromLeft (110));
    divide();
    speed.setBounds (r.removeFromLeft (90));

    bypass.setBounds (r.removeFromRight (84));
    r.removeFromRight (8);
    dividers.push_back ((float) r.getRight() - 4.0f);
    r.removeFromRight (16);
    multi.setBounds (r.removeFromRight (130));
}

} // namespace stdc
