#pragma once
#include "Display.h"

namespace stdc
{

/** The drum-part selector: one lit row per part (glyph and name). Picking a part switches the
    engine and moves the controls to that part's starting point (one undo step). */
class RoleSelector : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit RoleSelector (StDcProcessor&);
    ~RoleSelector() override { stopTimer(); }

    static constexpr int rowHeight = 44, headerHeight = 22;
    static int preferredHeight() { return headerHeight + kNumRoles * rowHeight; }

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    void mouseDown (const juce::MouseEvent&) override;

private:
    StDcProcessor& proc;
    int hover = -1, role = -1;
    st::Animated lit;   // selection glides between rows
    juce::Rectangle<float> rowBounds (int i) const;
    int rowAt (juce::Point<float>) const;
    void timerCallback() override;
};

//==============================================================================
/** The compressor controls, floating over the lower display: a translucent bar with grey buttons on its top edge
    (engine, auto release, character, auto gain, external side-chain) and knobs hanging from it. */
class ControlBar : public juce::Component
{
public:
    explicit ControlBar (StDcProcessor&);
    static constexpr int height = 140;

    void paint (juce::Graphics&) override;
    void resized() override;

    st::Knob* getKnob (const juce::String& paramId) const;

private:
    StDcProcessor& proc;
    std::unique_ptr<st::Knob> input, threshold, ratio, knee, range, attack, release, drive, mix, output, link, hpf;
    std::unique_ptr<st::Dropdown> engine, character;
    std::unique_ptr<st::ToggleChip> autoRelease, autoGain, external;
    juce::Rectangle<float> box;
    std::vector<float> dividers;
    std::vector<std::pair<juce::Rectangle<float>, juce::String>> groupCaptions;
};

//==============================================================================
/** Bottom tray: oversampling, display range / speed, the drum multiview, bypass. */
class FooterBar : public juce::Component
{
public:
    explicit FooterBar (StDcProcessor&);
    std::function<void()> onMultiView;
    std::function<void (float rangeDb, float seconds)> onDisplayChanged;
    void setDisplay (float rangeDb, float seconds);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    st::Dropdown quality;
    st::FlatButton range { "", "Level display range" }, speed { "", "How much time the level display shows" };
    st::FlatButton multi { "Drum Multiview", "Every drum track with ST DC, and the other ST plug-ins on those tracks" };
    st::ToggleChip bypass;
    float rangeDb = 48.0f, seconds = 4.0f;
    std::vector<float> dividers;
    void refreshText();
};

} // namespace stdc
