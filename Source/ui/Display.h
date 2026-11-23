#pragma once
#include "../PluginProcessor.h"
#include "Style.h"

namespace stdc
{

//==============================================================================
/** The main display (Pro-style level history): input level as a dark fill rising from the bottom, output level as
    a lighter fill with a stroke, gain reduction as a red trace hanging from the top, all on one dB scale, newest at
    the right. The threshold is a dashed line with a pill handle you drag vertically. */
class HistoryView : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit HistoryView (StDcProcessor&);

    void tick (double dt);
    /** dB shown from the top (0 dB) to the bottom (-range). */
    void setRangeDb (float r) { rangeDb = r; repaint(); }
    float getRangeDb() const noexcept { return rangeDb; }
    void setSeconds (float s) { seconds = s; repaint(); }
    /** x where the data ends (right edge of the plotted area; the knee panel sits to the right). */
    void setPlotRight (float x) { plotRight = x; }
    void setPlotLeft (float x) { plotLeft = x; }

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hoverHandle = false; repaint(); }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    StDcProcessor& proc;
    st::HistoryReader reader;
    st::SmoothScroll scroll;
    float rangeDb = 48.0f, seconds = 4.0f, plotLeft = 0.0f, plotRight = 0.0f;
    bool hoverHandle = false, dragging = false;
    float dragStartDb = 0.0f, dragStartY = 0.0f;

    juce::Rectangle<float> plot() const;
    float dbToY (float db) const;
    float yToDb (float y) const;
    juce::Rectangle<float> handleBounds() const;
};

//==============================================================================
/** Input spectrum (FFT) with the amount of compression on top of it: the band between the input spectrum and the
    input minus the current gain reduction is lit red. The side-chain high-pass is drawn as a dashed curve with a
    pill handle (drag sideways; double-click turns it off). */
class SpectrumView : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit SpectrumView (StDcProcessor&);
    void tick (double dt);

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    StDcProcessor& proc;
    st::SpectrumAnalyzer input, output;
    st::LiveLevels live;
    bool hover = false, dragging = false;
    static constexpr float minHz = 20.0f, maxHz = 20000.0f, minDb = -90.0f, maxDb = 0.0f;

    float hzToX (float hz) const;
    float xToHz (float x) const;
    float dbToY (float db) const;
    juce::Rectangle<float> handleBounds() const;
};

//==============================================================================
/** Transfer curve (input dB -> output dB) of the running engine, with the live detector level as a dot riding it. */
class KneeView : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit KneeView (StDcProcessor&);
    void tick (double dt);
    void paint (juce::Graphics&) override;

private:
    StDcProcessor& proc;
    st::LiveLevels live;
    float liveDb = -120.0f;
    CompSettings lastSettings;
    juce::Path curve;
    void rebuild (juce::Rectangle<float>);
};

//==============================================================================
/** Input, gain reduction and output meters (peak bars with hold and readouts). */
class MeterView : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit MeterView (StDcProcessor&);
    void tick (double dt);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    StDcProcessor& proc;
    st::LiveLevels live;
    std::array<float, 2> in { -120.0f, -120.0f }, out { -120.0f, -120.0f };
    float gr = 0.0f, inHold = -120.0f, outHold = -120.0f, grHold = 0.0f;
    double inHoldT = 0.0, outHoldT = 0.0, grHoldT = 0.0;
};

} // namespace stdc
