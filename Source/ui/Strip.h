#pragma once
#include "Display.h"

namespace stdc
{

/** ST DC's module in the Drum Multiview (st::MultiView via StDcProcessor::createStripView):
    the drum part (glyph, name, engine; click for a native menu to change it), a bypass switch, a live level / gain
    reduction history with the threshold, a GR readout and Threshold / Ratio / Attack / Release / Mix / Output knobs.
    Other ST plug-ins render it off screen and forward the mouse, so it is knobs and self-handled clicks only. */
class DcStrip : public juce::Component, private juce::Timer
{
public:
    explicit DcStrip (StDcProcessor&);
    ~DcStrip() override;

    static int widthFor (int height);
    int getNumKnobs() const noexcept { return (int) knobs.size(); }
    st::Knob* getKnob (int i) noexcept { return juce::isPositiveAndBelow (i, (int) knobs.size()) ? knobs[(size_t) i].get() : nullptr; }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    StDcProcessor& proc;
    HistoryReader reader;
    std::vector<std::unique_ptr<st::Knob>> knobs;
    juce::Rectangle<int> partArea, historyArea, powerArea;
    float gr = 0.0f;

    static int knobWidth (int height);
    static int historyWidth (int height);
    void timerCallback() override;
};

} // namespace stdc
