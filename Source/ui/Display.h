#pragma once
#include "../PluginProcessor.h"
#include "Style.h"

namespace stdc
{

/** Reads the engine's LevelHistory without consuming it and keeps the latest frames in a local ring, addressed by
    the history's absolute frame index.

    With `releaseDbPerSecond` > 0 the input / output levels are turned into a peak envelope as they arrive (instant
    attack, constant-rate release): raw 4 ms peaks of drums jump by several dB from frame to frame, an envelope reads
    as one calm line. Gain reduction is already smooth (compressor ballistics) and is kept as is. */
class HistoryReader
{
public:
    explicit HistoryReader (const LevelHistory& h, int capacity = 2048, float releaseDbPerSecond = 0.0f);
    /** Pulls new frames; returns how many arrived. */
    int update();
    /** Frames received so far (up to the capacity). */
    int size() const noexcept { return filled; }
    /** Absolute index one past the newest frame, and of the oldest frame still held. */
    uint32_t end() const noexcept { return last; }
    uint32_t begin() const noexcept { return last - (uint32_t) filled; }
    /** Frame by absolute index (must be in [begin, end)). */
    const LevelHistory::Frame& at (uint32_t index) const noexcept { return frames[(size_t) (index % (uint32_t) frames.size())]; }
    /** Frame `ago` frames before the newest (0 = newest). */
    const LevelHistory::Frame& get (int ago) const noexcept { return at (last - 1u - (uint32_t) ago); }
    double getFrameRate() const noexcept { return history.getFrameRate(); }
    /** Peaks (max levels, deepest GR) of the newest n frames. */
    LevelHistory::Frame peakOfNewest (int n) const noexcept;

private:
    const LevelHistory& history;
    std::vector<LevelHistory::Frame> frames;
    int filled = 0;
    uint32_t last = 0;
    bool primed = false;
    float releaseDb = 0.0f;
    LevelHistory::Frame env;
};

//==============================================================================
/** A display clock for a scrolling history: advances smoothly at the frame rate every UI frame and follows the
    audio (which arrives in bursts, one host block at a time) a little behind, correcting drift gently. Frames are
    drawn at `position`, so the picture glides instead of stepping by a block at a time. */
class SmoothScroll
{
public:
    /** Returns the display position (absolute frame index, fractional) for this UI frame. */
    double update (double dt, uint32_t newestEnd, double frameRate);
    double getPosition() const noexcept { return position; }

private:
    double position = 0.0, lastEnd = -1.0, lag = 6.0;
    bool started = false;
};

/** Paths of a level history: input / output fills, the output line and the gain-reduction trace (+ its fill).
    Buckets of frames are anchored to absolute frame indices, so the shape doesn't shimmer as it scrolls. */
struct HistoryPaths
{
    juce::Path inFill, outFill, outLine, grLine, grFill;
    bool any = false;

    /** levelY / grY map dB to y. x of absolute frame f = right - (position - f) / framesPerPixel. */
    void build (const HistoryReader&, double position, float left, float right, double framesPerPixel,
                const std::function<float (float)>& levelY, const std::function<float (float)>& grY,
                float bottom, float grTop);
};

//==============================================================================
/** Live values for meters / readouts, read from the history (so no hit between two UI frames is missed) with
    display ballistics: instant attack, smooth release. */
class LiveLevels
{
public:
    explicit LiveLevels (const LevelHistory& h) : reader (h, 256, 0.0f) {}
    /** Returns true when a value moved visibly. */
    bool update (double dt, float releaseSeconds = 0.18f);
    float grDb = 0.0f, detDb = -120.0f, inDb = -120.0f, outDb = -120.0f;

private:
    HistoryReader reader;
};

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
    HistoryReader reader;
    SmoothScroll scroll;
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
    LiveLevels live;
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
    LiveLevels live;
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
    LiveLevels live;
    std::array<float, 2> in { -120.0f, -120.0f }, out { -120.0f, -120.0f };
    float gr = 0.0f, inHold = -120.0f, outHold = -120.0f, grHold = 0.0f;
    double inHoldT = 0.0, outHoldT = 0.0, grHoldT = 0.0;
};

} // namespace stdc
