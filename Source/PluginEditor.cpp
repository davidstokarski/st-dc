#include "PluginEditor.h"

using namespace stdc;

StDcEditor::StDcEditor (StDcProcessor& p)
    : EditorBase (p, designWidth, designHeight),
      proc (p), history (p), spectrum (p), knee (p), meters (p), roles (p), controls (p), footer (p)
{
    header.setProductName ("DC");
    header.setMultiViewCallback ([this] { toggleMultiView(); });

    auto& content = getContent();
    for (juce::Component* c : { (juce::Component*) &history, (juce::Component*) &spectrum, (juce::Component*) &knee,
                                (juce::Component*) &meters, (juce::Component*) &roles, (juce::Component*) &controls,
                                (juce::Component*) &footer })
        content.addAndMakeVisible (c);

    controls.setBufferedToImage (true);
    footer.setBufferedToImage (true);
    header.setBufferedToImage (true);
    header.toFront (false);

    footer.onMultiView = [this] { toggleMultiView(); };
    footer.onDisplayChanged = [this] (float r, float s) { setDisplay (r, s); };
    setDisplay ((float) (double) proc.uiState.getProperty ("displayRange", 48.0), (float) (double) proc.uiState.getProperty ("displaySeconds", 4.0));

    clock = std::make_unique<st::FrameClock> (history, [this] (double dt)
    {
        history.tick (dt);
        spectrum.tick (dt);
        knee.tick (dt);
        meters.tick (dt);
    });
    relayout();
}

StDcEditor::~StDcEditor()
{
    clock.reset();
    multiView.reset();
}

void StDcEditor::setDisplay (float rangeDb, float seconds)
{
    proc.uiState.setProperty ("displayRange", rangeDb, nullptr);
    proc.uiState.setProperty ("displaySeconds", seconds, nullptr);
    history.setRangeDb (rangeDb);
    history.setSeconds (seconds);
    footer.setDisplay (rangeDb, seconds);
}

void StDcEditor::toggleMultiView()
{
    if (multiView != nullptr) { multiView.reset(); return; }
    st::MultiView::Options o;
    o.title = "Drum Multiview";
    o.requiredKind = "ST DC";    // only drum tracks: tracks with an ST DC ...
    o.leftmostKind = "ST DC";    // ... with every other ST plug-in on them; the compressors line up in one column
    o.orderFor = [] (st::ProcessorBase& p)
    {
        // rows in kit order: bus, kick, snare, toms, cymbals, room, verb
        if (auto* dc = dynamic_cast<StDcProcessor*> (&p)) return dc->getRole();
        return kNumRoles;
    };
    multiView = std::make_unique<st::MultiView> (proc, o);
    multiView->onClose = [this]
    {
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<StDcEditor> (this)]
        {
            if (safe != nullptr && safe->multiView != nullptr) safe->toggleMultiView();
        });
    };
    getContent().addAndMakeVisible (*multiView);
    multiView->setBounds (getContent().getLocalBounds());
    multiView->toFront (true);
}

void StDcEditor::layout (juce::Rectangle<int>)
{
    const auto all = getContent().getLocalBounds();
    const int inner = designWidth - 2 * cheek;
    history.setBounds (cheek, historyY, inner, historyH);
    roles.setBounds (cheek + 8, historyY + 12, 118, RoleSelector::preferredHeight());
    history.setPlotLeft ((float) (roles.getRight() - cheek + 6));
    history.setPlotRight ((float) (inner - 184));
    knee.setBounds (designWidth - cheek - 170, historyY + 14, 156, 156);
    meters.setBounds (knee.getX(), knee.getBottom() + 12, 156, historyY + historyH - knee.getBottom() - 18);
    spectrum.setBounds (cheek, spectrumY, inner, spectrumH);
    controls.setBounds (cheek + 4, controlsY, inner - 8, ControlBar::height);
    footer.setBounds (all.withTop (designHeight - footerHeight));
    if (multiView != nullptr) multiView->setBounds (all);
}

void StDcEditor::paintBackground (juce::Graphics& g, juce::Rectangle<int> area)
{
    // static: render once per physical scale, blit afterwards (it sits behind everything that animates)
    const float pixelScale = juce::jmax (1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    const auto full = getContent().getLocalBounds();
    if (backdrop.isNull() || std::abs (pixelScale - backdropScale) > 0.01f)
    {
        backdropScale = pixelScale;
        backdrop = juce::Image (juce::Image::RGB, juce::roundToInt ((float) full.getWidth() * pixelScale),
                                juce::roundToInt ((float) full.getHeight() * pixelScale), false);
        juce::Graphics bg (backdrop);
        bg.addTransform (juce::AffineTransform::scale (pixelScale));
        renderBackdrop (bg, full.toFloat());
    }
    g.drawImage (backdrop, area.toFloat(), juce::RectanglePlacement::stretchToFit);
}

void StDcEditor::renderBackdrop (juce::Graphics& g, juce::Rectangle<float> r)
{
    const auto& t = st::Theme::get();
    // walnut under a deep lacquer: the grain shows faintly through the display
    style::paintWood (g, r, 0.0f);
    juce::ColourGradient lacquer (t.displayTop.withAlpha (0.80f), r.getCentreX(), r.getY(), t.bg0.withAlpha (0.93f), r.getCentreX(), r.getBottom(), false);
    lacquer.addColour (0.5, t.displayTop.interpolatedWith (t.bg0, 0.6f).withAlpha (0.86f));
    g.setGradientFill (lacquer);
    g.fillRect (r);
    // warm light from the top centre, vignette towards the corners
    g.setGradientFill (juce::ColourGradient (t.displayGlow.withAlpha (0.16f), r.getCentreX(), r.getY() + 40.0f,
                                             juce::Colours::transparentBlack, r.getCentreX(), r.getY() + r.getHeight() * 0.6f, true));
    g.fillRect (r);
    g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, r.getCentreX(), r.getY() + r.getHeight() * 0.4f,
                                             juce::Colours::black.withAlpha (0.35f), r.getX(), r.getY() + r.getHeight() * 0.4f, true));
    g.fillRect (r);

    // a faded divider between the level display and the spectrum
    st::paint::fadedLine (g, { (float) cheek + 20.0f, (float) spectrumY - 2.5f }, { r.getRight() - (float) cheek - 20.0f, (float) spectrumY - 2.5f },
                          juce::Colours::white.withAlpha (0.07f), 1.0f, 0.12f);

    // wooden cheeks
    style::paintCheek (g, { r.getX(), r.getY(), (float) cheek, r.getHeight() }, true);
    style::paintCheek (g, { r.getRight() - (float) cheek, r.getY(), (float) cheek, r.getHeight() }, false);
}
