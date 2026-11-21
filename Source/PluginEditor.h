#pragma once
#include "PluginProcessor.h"
#include "ui/Display.h"
#include "ui/Panels.h"

class StDcEditor : public st::EditorBase
{
public:
    explicit StDcEditor (StDcProcessor&);
    ~StDcEditor() override;

    static constexpr int designWidth = 1040, designHeight = 690;
    static constexpr int historyY = 44, historyH = 352, spectrumY = 398, spectrumH = 104,
                         controlsY = 504, footerHeight = 40;

    /** Opens the Drum Multiview (every track with ST DC, plus the other ST plug-ins on those tracks). */
    void openMultiView() { if (multiView == nullptr) toggleMultiView(); }
    st::MultiView* getMultiView() noexcept { return multiView.get(); }
    stdc::ControlBar& getControlBar() noexcept { return controls; }

private:
    StDcProcessor& proc;
    stdc::HistoryView history;
    stdc::SpectrumView spectrum;
    stdc::KneeView knee;
    stdc::MeterView meters;
    stdc::RoleSelector roles;
    stdc::ControlBar controls;
    stdc::FooterBar footer;
    std::unique_ptr<st::MultiView> multiView;
    std::unique_ptr<st::FrameClock> clock;
    juce::Image backdrop;
    float backdropScale = 0.0f;

    void layout (juce::Rectangle<int> body) override;
    void paintBackground (juce::Graphics&, juce::Rectangle<int> area) override;
    void renderBackdrop (juce::Graphics&, juce::Rectangle<float> full);
    void toggleMultiView();
    void setDisplay (float rangeDb, float seconds);
};
