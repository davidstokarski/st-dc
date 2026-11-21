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
    const int inner = designWidth;
    history.setBounds (0, historyY, inner, historyH);
    roles.setBounds (16, historyY + 12, 118, RoleSelector::preferredHeight());
    history.setPlotLeft ((float) (roles.getRight() + 6));
    history.setPlotRight ((float) (inner - 184));
    knee.setBounds (designWidth - 176, historyY + 14, 156, 156);
    meters.setBounds (knee.getX(), knee.getBottom() + 12, 156, historyY + historyH - knee.getBottom() - 18);
    spectrum.setBounds (0, spectrumY, inner, spectrumH);
    controls.setBounds (12, controlsY, inner - 24, ControlBar::height);
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
    style::paintBackdrop (g, r);
    // a faded divider between the level display and the spectrum
    st::paint::fadedLine (g, { r.getX() + 24.0f, (float) spectrumY - 2.5f }, { r.getRight() - 24.0f, (float) spectrumY - 2.5f },
                          juce::Colours::white.withAlpha (0.06f), 1.0f, 0.18f);
}
