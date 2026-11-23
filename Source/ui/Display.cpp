#include "Display.h"

namespace stdc
{

//==============================================================================
namespace
{
    juce::Colour inputFill()  { return st::Theme::get().textFaint.withAlpha (0.20f); }
    juce::Colour outputFill() { return st::Theme::get().textDim.withAlpha (0.22f); }
    juce::Colour outputLine() { return st::Theme::get().text.withAlpha (0.62f); }

    void dashedLine (juce::Graphics& g, juce::Line<float> l, juce::Colour c, float thickness = 1.0f)
    {
        const float dashes[] = { 4.0f, 4.0f };
        g.setColour (c);
        g.drawDashedLine (l, dashes, 2, thickness);
    }
}

HistoryView::HistoryView (StDcProcessor& p) : proc (p), reader (p.getEngine().history, 4096, 260.0f)
{
    setTooltip ("Level history: input (dark), output (light) and gain reduction (red). Drag the threshold handle.");
}

void HistoryView::tick (double dt)
{
    reader.update();
    const double before = scroll.getPosition();
    const double pos = scroll.update (dt, reader.end(), reader.getFrameRate());
    if (std::abs (pos - before) > 1.0e-3 || dragging) repaint();
}

juce::Rectangle<float> HistoryView::plot() const
{
    const float right = plotRight > 0.0f ? plotRight : (float) getWidth();
    return { plotLeft, 0.0f, juce::jmax (10.0f, right - plotLeft), (float) getHeight() };
}

float HistoryView::dbToY (float db) const
{
    const auto r = plot().reduced (0.0f, 10.0f);
    return juce::jmap (juce::jlimit (-rangeDb, 0.0f, db), -rangeDb, 0.0f, r.getBottom(), r.getY());
}

float HistoryView::yToDb (float y) const
{
    const auto r = plot().reduced (0.0f, 10.0f);
    return juce::jmap (y, r.getBottom(), r.getY(), -rangeDb, 0.0f);
}

juce::Rectangle<float> HistoryView::handleBounds() const
{
    const auto r = plot();
    const float th = proc.apvts.getRawParameterValue (ids::threshold)->load();
    return juce::Rectangle<float> (36.0f, 14.0f).withCentre ({ r.getRight() - 64.0f, dbToY (th) });
}

void HistoryView::paint (juce::Graphics& g)
{
    const auto& t = st::Theme::get();
    const auto r = plot();
    const auto inner = r.reduced (0.0f, 10.0f);

    // dB grid (faded at both ends) + labels on the right
    const float step = rangeDb <= 30.0f ? 3.0f : (rangeDb <= 50.0f ? 6.0f : 12.0f);
    g.setFont (st::fonts::regular (10.0f));
    for (float db = -step; db > -rangeDb + 0.1f; db -= step)
    {
        const float y = dbToY (db);
        st::paint::fadedLine (g, { r.getX(), y }, { r.getRight() - 30.0f, y }, juce::Colours::white.withAlpha (0.045f), 1.0f, 0.08f);
        g.setColour (t.textFaint.withAlpha (0.8f));
        g.drawText (juce::String ((int) db), juce::Rectangle<float> (r.getRight() - 28.0f, y - 7.0f, 26.0f, 14.0f), juce::Justification::centredRight, false);
    }

    // ---- data: newest at the right, gliding with the display clock
    const float W = juce::jmax (1.0f, r.getWidth() - 34.0f);
    const double fpp = juce::jmax (0.05, (double) seconds * reader.getFrameRate() / (double) W);
    const float bottom = inner.getBottom(), top = inner.getY();
    st::HistoryPaths paths;
    paths.build (reader, scroll.getPosition(), r.getX() - 4.0f, r.getX() + W, fpp,
                 [this] (float db) { return dbToY (db); },
                 [&] (float gr)
                 {
                     // GR hangs from the top on the levels' dB scale; with no reduction it tucks above the edge (hidden)
                     const float depth = juce::jlimit (0.0f, rangeDb, -gr);
                     return juce::jmap (depth, 0.0f, rangeDb, top, bottom) - (top - r.getY() + 2.0f) * (1.0f - juce::jlimit (0.0f, 1.0f, depth / 0.5f));
                 },
                 bottom, r.getY() - 2.0f);

    if (paths.any)
    {
        g.saveState();
        g.reduceClipRegion (r.toNearestInt());
        // data fades in from the left edge (no hard start)
        auto fade = [&] (juce::Colour c)
        {
            juce::ColourGradient grad (c.withAlpha (0.0f), r.getX(), 0.0f, c, r.getX() + 70.0f, 0.0f, false);
            return grad;
        };
        g.setGradientFill (fade (inputFill()));
        g.fillPath (paths.inFill);
        g.setGradientFill (fade (outputFill()));
        g.fillPath (paths.outFill);
        g.setGradientFill (fade (outputLine()));
        g.strokePath (paths.outLine, juce::PathStrokeType (1.1f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setGradientFill (fade (style::grColour().withAlpha (0.13f)));
        g.fillPath (paths.grFill);
        g.setGradientFill (fade (style::grColour()));
        g.strokePath (paths.grLine, juce::PathStrokeType (1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.restoreState();
    }

    // ---- threshold line + handle
    {
        const float th = proc.apvts.getRawParameterValue (ids::threshold)->load();
        const float y = dbToY (th);
        dashedLine (g, { r.getX() + 40.0f, y, r.getRight() - 140.0f, y }, t.accent.withAlpha (hoverHandle || dragging ? 0.85f : 0.55f));
        st::paint::fadedLine (g, { r.getX(), y }, { r.getX() + 40.0f, y }, t.accent.withAlpha (0.4f), 1.0f, 0.5f);
        const auto hb = handleBounds();
        st::paint::pillHandle (g, hb, hoverHandle, dragging);
        const auto label = st::params::formatDb (th);
        g.setFont (st::fonts::medium (11.0f));
        const float lw = st::fonts::stringWidth (st::fonts::medium (11.0f), label) + 10.0f;
        const auto lr = juce::Rectangle<float> (hb.getX() - 8.0f - lw, y - 8.0f, lw, 16.0f);
        g.setColour (t.bg0.withAlpha (0.75f));
        g.fillRoundedRectangle (lr, 4.0f);
        g.setColour (hoverHandle || dragging ? t.text : t.textDim);
        g.drawText (label, lr, juce::Justification::centred, false);
    }

    // ---- engine caption (top left of the plot)
    {
        const auto algo = (int) proc.getAlgorithm();
        const auto& info = getAlgorithmInfo (algo);
        const juce::String name (info.name), sub (juce::String (juce::CharPointer_UTF8 ("\xc2\xb7  ")) + info.character);
        const float w1 = st::fonts::stringWidth (st::fonts::semibold (13.0f), name);
        const float w2 = st::fonts::stringWidth (st::fonts::regular (12.0f), sub);
        auto row = juce::Rectangle<float> (r.getX() + 12.0f, 8.0f, w1 + w2 + 26.0f, 20.0f);
        g.setColour (t.bg0.withAlpha (0.62f));
        g.fillRoundedRectangle (row, 5.0f);
        row.reduce (9.0f, 0.0f);
        g.setFont (st::fonts::semibold (13.0f));
        g.setColour (t.accent2);
        g.drawText (name, row, juce::Justification::centredLeft, false);
        g.setFont (st::fonts::regular (12.0f));
        g.setColour (t.textDim);
        g.drawText (sub, row.withTrimmedLeft (w1 + 8.0f), juce::Justification::centredLeft, false);
    }
}

void HistoryView::mouseMove (const juce::MouseEvent& e)
{
    const bool h = handleBounds().expanded (6.0f, 5.0f).contains (e.position);
    if (h != hoverHandle) { hoverHandle = h; repaint(); }
    setMouseCursor (h ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor);
}

void HistoryView::mouseDown (const juce::MouseEvent& e)
{
    if (! handleBounds().expanded (6.0f, 5.0f).contains (e.position)) return;
    dragging = true;
    dragStartDb = proc.apvts.getRawParameterValue (ids::threshold)->load();
    dragStartY = e.position.y;
    proc.param (ids::threshold).beginChangeGesture();
    proc.undoManager.beginNewTransaction ("Threshold");
}

void HistoryView::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging) return;
    const auto in = plot().reduced (0.0f, 10.0f);
    const float dbPerPx = rangeDb / in.getHeight() * (e.mods.isShiftDown() ? 0.2f : 1.0f);
    const float db = juce::jlimit (-60.0f, 0.0f, dragStartDb - (e.position.y - dragStartY) * dbPerPx);
    auto& p = proc.param (ids::threshold);
    p.setValueNotifyingHost (p.convertTo0to1 (db));
    repaint();
}

void HistoryView::mouseUp (const juce::MouseEvent&)
{
    if (! dragging) return;
    dragging = false;
    proc.param (ids::threshold).endChangeGesture();
    repaint();
}

void HistoryView::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! handleBounds().expanded (6.0f, 5.0f).contains (e.position)) return;
    auto& p = proc.param (ids::threshold);
    p.beginChangeGesture();
    p.setValueNotifyingHost (p.getDefaultValue());
    p.endChangeGesture();
}

void HistoryView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (! handleBounds().expanded (10.0f, 8.0f).contains (e.position)) return;
    auto& p = proc.param (ids::threshold);
    const float db = proc.apvts.getRawParameterValue (ids::threshold)->load() + (w.deltaY > 0 ? 0.5f : -0.5f);
    p.beginChangeGesture();
    p.setValueNotifyingHost (p.convertTo0to1 (juce::jlimit (-60.0f, 0.0f, db)));
    p.endChangeGesture();
}

//==============================================================================
namespace
{
    st::SpectrumAnalyzer::Options spectrumOptions()
    {
        st::SpectrumAnalyzer::Options o;
        o.fftOrder = 12;
        o.hopSize = 512;
        o.numDisplayPoints = 320;
        o.minHz = 20.0f;
        o.maxHz = 20000.0f;
        o.slopeDbPerOct = 4.5f;
        o.releaseDbPerSec = 36.0f;
        o.nonConsuming = true;
        return o;
    }
}

SpectrumView::SpectrumView (StDcProcessor& p)
    : proc (p), input (p.preAnalyzer, spectrumOptions()), output (p.postAnalyzer, spectrumOptions()), live (p.getEngine().history)
{
    setTooltip ("Input spectrum. Red: what the compressor takes away right now. Drag the handle to high-pass the side-chain.");
}

float SpectrumView::hzToX (float hz) const
{
    const auto r = getLocalBounds().toFloat();
    return r.getX() + r.getWidth() * std::log (hz / minHz) / std::log (maxHz / minHz);
}

float SpectrumView::xToHz (float x) const
{
    const auto r = getLocalBounds().toFloat();
    return minHz * std::pow (maxHz / minHz, (x - r.getX()) / r.getWidth());
}

float SpectrumView::dbToY (float db) const
{
    const auto r = getLocalBounds().toFloat().withTrimmedTop (6.0f).withTrimmedBottom (16.0f);
    return juce::jmap (juce::jlimit (minDb - 20.0f, maxDb, db), minDb, maxDb, r.getBottom(), r.getY());
}

juce::Rectangle<float> SpectrumView::handleBounds() const
{
    const float hz = proc.apvts.getRawParameterValue (ids::scHighPass)->load();
    return juce::Rectangle<float> (34.0f, 14.0f).withCentre ({ juce::jmax (36.0f, hzToX (hz)), 16.0f });
}

void SpectrumView::tick (double dt)
{
    bool changed = input.update (dt);
    changed |= output.update (dt);
    changed |= live.update (dt, 0.2f);
    if (changed) repaint();
}

void SpectrumView::paint (juce::Graphics& g)
{
    const auto& t = st::Theme::get();
    const auto r = getLocalBounds().toFloat();
    const float plotBottom = r.getBottom() - 16.0f;

    // grid: decades and labels
    g.setFont (st::fonts::regular (10.0f));
    for (float hz : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const float x = hzToX (hz);
        st::paint::fadedLine (g, { x, r.getY() + 4.0f }, { x, plotBottom }, juce::Colours::white.withAlpha (0.04f), 1.0f, 0.25f);
        g.setColour (t.textFaint.withAlpha (0.85f));
        g.drawText (hz >= 1000.0f ? juce::String ((int) (hz / 1000.0f)) + "k" : juce::String ((int) hz),
                    juce::Rectangle<float> (x - 20.0f, plotBottom + 1.0f, 40.0f, 14.0f), juce::Justification::centred, false);
    }

    // side-chain high-pass: the region the detector doesn't hear is shaded, and its response is a dashed curve
    const float hpf = proc.apvts.getRawParameterValue (ids::scHighPass)->load();
    const bool hpfOn = hpf > 20.5f;
    if (hpfOn)
    {
        const float xc = hzToX (hpf);
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.22f), r.getX(), 0.0f,
                                                 juce::Colours::black.withAlpha (0.0f), xc, 0.0f, false));
        g.fillRect (r.withRight (xc).withBottom (plotBottom));
        juce::Path resp;
        bool started = false;
        for (float x = r.getX(); x <= xc * 2.2f && x <= r.getRight(); x += 2.0f)
        {
            const float f = xToHz (x) / hpf;
            const float magDb = -10.0f * std::log10 (1.0f + std::pow (f, -8.0f));   // 24 dB/oct Butterworth-ish
            if (magDb > -0.15f) break;
            const float y = juce::jmap (juce::jlimit (-30.0f, 0.0f, magDb), -30.0f, 0.0f, plotBottom, r.getY() + 30.0f);
            if (! started) { resp.startNewSubPath (x, y); started = true; } else resp.lineTo (x, y);
        }
        const float dashes[] = { 3.0f, 3.0f };
        juce::Path dashed;
        juce::PathStrokeType (1.2f).createDashedStroke (dashed, resp, dashes, 2);
        g.setColour (t.accent.withAlpha (0.75f));
        g.fillPath (dashed);
    }

    // spectra
    if (input.getNumPoints() > 1)
    {
        const auto& lv = input.getLevelsDb();
        const int n = input.getNumPoints();
        juce::Path top, fill, band;
        for (int i = 0; i < n; ++i)
        {
            const float x = hzToX (input.getFrequencyForPoint (i));
            const float y = dbToY (lv[(size_t) i]);
            if (i == 0) { top.startNewSubPath (x, y); fill.startNewSubPath (x, plotBottom); }
            else top.lineTo (x, y);
            fill.lineTo (x, y);
        }
        fill.lineTo (hzToX (input.getFrequencyForPoint (n - 1)), plotBottom);
        fill.closeSubPath();

        g.setGradientFill (juce::ColourGradient (t.spectrum.withAlpha (0.20f), 0.0f, r.getY(), t.spectrum.withAlpha (0.04f), 0.0f, plotBottom, false));
        g.fillPath (fill);
        g.setColour (t.spectrum.withAlpha (0.55f));
        g.strokePath (top, juce::PathStrokeType (1.0f));

        // the compression: input minus the current gain reduction
        if (live.grDb < -0.05f)
        {
            juce::Path reduced;
            for (int i = 0; i < n; ++i)
            {
                const float x = hzToX (input.getFrequencyForPoint (i));
                const float y = dbToY (lv[(size_t) i] + live.grDb);
                if (i == 0) reduced.startNewSubPath (x, y); else reduced.lineTo (x, y);
            }
            band = top;
            for (int i = n - 1; i >= 0; --i)
                band.lineTo (hzToX (input.getFrequencyForPoint (i)), dbToY (lv[(size_t) i] + live.grDb));
            band.closeSubPath();
            g.setColour (style::grColour().withAlpha (0.34f));
            g.fillPath (band);
            g.setColour (style::grColour().withAlpha (0.85f));
            g.strokePath (reduced, juce::PathStrokeType (1.1f));
        }
    }
    if (output.getNumPoints() > 1)
    {
        g.setColour (t.text.withAlpha (0.28f));
        g.strokePath (output.createPath (r.withTrimmedTop (6.0f).withBottom (plotBottom), minDb, maxDb, minHz, maxHz, false), juce::PathStrokeType (0.9f));
    }

    // handle + label
    const auto hb = handleBounds();
    st::paint::pillHandle (g, hb, hover, dragging);
    g.setFont (st::fonts::medium (11.0f));
    g.setColour (hover || dragging ? t.text : t.textDim);
    g.drawText ("SC HPF  " + (hpfOn ? juce::String (juce::roundToInt (hpf)) + " Hz" : juce::String ("Off")),
                juce::Rectangle<float> (hb.getRight() + 6.0f, hb.getY() - 1.0f, 120.0f, 16.0f), juce::Justification::centredLeft, false);

    // GR readout (right)
    g.setFont (st::fonts::semibold (11.0f));
    g.setColour (live.grDb < -0.05f ? style::grColour() : t.textFaint);
    g.drawText ("GR " + st::params::formatDb (live.grDb, 1, true, false), juce::Rectangle<float> (r.getRight() - 110.0f, 6.0f, 100.0f, 16.0f),
                juce::Justification::centredRight, false);
}

void SpectrumView::mouseMove (const juce::MouseEvent& e)
{
    const bool h = handleBounds().expanded (6.0f).contains (e.position);
    if (h != hover) { hover = h; repaint(); }
    setMouseCursor (h ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
}

void SpectrumView::mouseDown (const juce::MouseEvent& e)
{
    if (! handleBounds().expanded (6.0f).contains (e.position)) return;
    dragging = true;
    proc.undoManager.beginNewTransaction ("Side-chain high-pass");
    proc.param (ids::scHighPass).beginChangeGesture();
}

void SpectrumView::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging) return;
    auto& p = proc.param (ids::scHighPass);
    p.setValueNotifyingHost (p.convertTo0to1 (juce::jlimit (20.0f, 500.0f, xToHz (e.position.x))));
    repaint();
}

void SpectrumView::mouseUp (const juce::MouseEvent&)
{
    if (! dragging) return;
    dragging = false;
    proc.param (ids::scHighPass).endChangeGesture();
    repaint();
}

void SpectrumView::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! handleBounds().expanded (6.0f).contains (e.position)) return;
    auto& p = proc.param (ids::scHighPass);
    p.beginChangeGesture();
    p.setValueNotifyingHost (0.0f);
    p.endChangeGesture();
}

//==============================================================================
KneeView::KneeView (StDcProcessor& p) : proc (p), live (p.getEngine().history)
{
    setTooltip ("Transfer curve of the running engine. The dot is the level the detector hears right now.");
}

void KneeView::tick (double dt)
{
    const auto s = proc.getCompSettings();
    auto differs = [] (float a, float b) { return std::abs (a - b) > 1.0e-4f; };
    const bool changed = s.algorithm != lastSettings.algorithm || differs (s.thresholdDb, lastSettings.thresholdDb)
                         || differs (s.ratio, lastSettings.ratio) || differs (s.kneeDb, lastSettings.kneeDb)
                         || differs (s.rangeDb, lastSettings.rangeDb);
    if (changed) { lastSettings = s; curve.clear(); }
    const bool moved = live.update (dt, 0.25f);
    liveDb = live.detDb;
    if (changed || moved) repaint();
}

void KneeView::rebuild (juce::Rectangle<float> a)
{
    curve.clear();
    constexpr float lo = -60.0f;
    for (int i = 0; i <= 120; ++i)
    {
        const float in = lo + (0.0f - lo) * (float) i / 120.0f;
        const float out = in + Compressor::staticGainDb (in, lastSettings);
        const float x = juce::jmap (in, lo, 0.0f, a.getX(), a.getRight());
        const float y = juce::jmap (juce::jlimit (lo - 6.0f, 6.0f, out), lo, 0.0f, a.getBottom(), a.getY());
        if (i == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
    }
}

void KneeView::paint (juce::Graphics& g)
{
    const auto& t = st::Theme::get();
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.38f));
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.0f);

    const auto a = r.reduced (10.0f);
    constexpr float lo = -60.0f;
    for (float db = -48.0f; db < 0.0f; db += 12.0f)
    {
        const float x = juce::jmap (db, lo, 0.0f, a.getX(), a.getRight());
        const float y = juce::jmap (db, lo, 0.0f, a.getBottom(), a.getY());
        g.setColour (juce::Colours::white.withAlpha (0.04f));
        g.fillRect (x, a.getY(), 1.0f, a.getHeight());
        g.fillRect (a.getX(), y, a.getWidth(), 1.0f);
    }
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawLine (a.getX(), a.getBottom(), a.getRight(), a.getY(), 1.0f);

    if (curve.isEmpty()) rebuild (a);
    // threshold marker
    const float tx = juce::jmap (lastSettings.thresholdDb, lo, 0.0f, a.getX(), a.getRight());
    st::paint::fadedLine (g, { tx, a.getY() }, { tx, a.getBottom() }, t.accent.withAlpha (0.45f), 1.0f, 0.2f);

    g.saveState();
    g.reduceClipRegion (a.expanded (2.0f).toNearestInt());
    st::paint::glowStroke (g, curve, t.curve.withAlpha (0.9f), 1.6f, 3.0f);
    g.restoreState();

    // live dot
    if (liveDb > lo)
    {
        const float out = liveDb + Compressor::staticGainDb (liveDb, lastSettings);
        const juce::Point<float> p (juce::jmap (liveDb, lo, 0.0f, a.getX(), a.getRight()),
                                    juce::jmap (juce::jlimit (lo, 0.0f, out), lo, 0.0f, a.getBottom(), a.getY()));
        g.setColour (t.accent2.withAlpha (0.25f));
        g.fillEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre (p));
        g.setColour (t.accent2);
        g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (p));
    }

    st::paint::caption (g, "Knee", r.removeFromTop (20.0f).reduced (10.0f, 0.0f).translated (0.0f, 3.0f), t.textFaint,
                        juce::Justification::centredLeft, 9.5f);
}

//==============================================================================
MeterView::MeterView (StDcProcessor& p) : proc (p), live (p.getEngine().history)
{
    setTooltip ("Input, gain reduction and output (peak). Click to reset the hold values.");
}

void MeterView::mouseDown (const juce::MouseEvent&)
{
    inHold = outHold = -120.0f;
    grHold = 0.0f;
    repaint();
}

void MeterView::tick (double dt)
{
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const float fall = (float) dt * 26.0f;
    auto rin = proc.inputMeter.read(), rout = proc.outputMeter.read();
    for (int c = 0; c < 2; ++c)
    {
        const int ci = juce::jmin (c, rin.numChannels - 1), co = juce::jmin (c, rout.numChannels - 1);
        in[(size_t) c] = juce::jmax (st::dsp::gainToDb (rin.peak[(size_t) ci]), in[(size_t) c] - fall);
        out[(size_t) c] = juce::jmax (st::dsp::gainToDb (rout.peak[(size_t) co]), out[(size_t) c] - fall);
    }
    live.update (dt, 0.12f);
    gr = live.grDb;
    const float i = juce::jmax (in[0], in[1]), o = juce::jmax (out[0], out[1]);
    if (i >= inHold || now - inHoldT > 2.0) { inHold = i; inHoldT = now; }
    if (o >= outHold || now - outHoldT > 2.0) { outHold = o; outHoldT = now; }
    if (gr <= grHold || now - grHoldT > 2.0) { grHold = gr; grHoldT = now; }
    repaint();
}

void MeterView::paint (juce::Graphics& g)
{
    const auto& t = st::Theme::get();
    auto r = getLocalBounds().toFloat();
    const float colW = r.getWidth() / 3.0f;
    auto readout = r.removeFromTop (16.0f);
    auto caption = r.removeFromBottom (16.0f);
    auto bars = r.reduced (0.0f, 4.0f);

    auto levelY = [&] (float db) { return juce::jmap (juce::jlimit (-60.0f, 6.0f, db), -60.0f, 6.0f, bars.getBottom(), bars.getY()); };

    auto drawLevel = [&] (int col, const std::array<float, 2>& lv, float hold, const char* name)
    {
        const float cx = bars.getX() + colW * ((float) col + 0.5f);
        for (int c = 0; c < 2; ++c)
        {
            auto slot = juce::Rectangle<float> (cx - 7.0f + 8.0f * (float) c, bars.getY(), 6.0f, bars.getHeight());
            g.setColour (juce::Colours::black.withAlpha (0.4f));
            g.fillRoundedRectangle (slot, 2.0f);
            const float y = levelY (lv[(size_t) c]);
            auto fill = slot.withTop (y);
            g.setGradientFill (juce::ColourGradient (t.accent2, 0.0f, bars.getY(), t.textDim.withAlpha (0.8f), 0.0f, bars.getY() + bars.getHeight() * 0.3f, false));
            if (lv[(size_t) c] > 0.0f) g.setColour (t.danger);
            g.fillRoundedRectangle (fill, 2.0f);
        }
        g.setFont (st::fonts::medium (10.5f));
        g.setColour (hold > 0.0f ? t.danger : t.text.withAlpha (0.85f));
        g.drawText (hold > -99.0f ? juce::String (hold, 1) : juce::String ("-inf"), readout.withX (cx - colW * 0.5f).withWidth (colW), juce::Justification::centred, false);
        st::paint::caption (g, name, caption.withX (cx - colW * 0.5f).withWidth (colW), t.textFaint, juce::Justification::centred, 9.5f);
    };

    drawLevel (0, in, inHold, "In");
    drawLevel (2, out, outHold, "Out");

    // gain reduction: red bar hanging from the top, 0..-24 dB
    {
        const float cx = bars.getX() + colW * 1.5f;
        auto slot = juce::Rectangle<float> (cx - 5.0f, bars.getY(), 10.0f, bars.getHeight());
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillRoundedRectangle (slot, 2.0f);
        const float depth = juce::jlimit (0.0f, 1.0f, -gr / 24.0f);
        g.setColour (style::grColour());
        g.fillRoundedRectangle (slot.withHeight (slot.getHeight() * depth), 2.0f);
        const float hy = slot.getY() + slot.getHeight() * juce::jlimit (0.0f, 1.0f, -grHold / 24.0f);
        g.setColour (style::grColour().withAlpha (0.8f));
        g.fillRect (slot.getX(), hy - 1.0f, slot.getWidth(), 1.5f);
        for (float db : { 6.0f, 12.0f, 18.0f })
        {
            const float y = slot.getY() + slot.getHeight() * db / 24.0f;
            g.setColour (juce::Colours::white.withAlpha (0.12f));
            g.fillRect (slot.getRight() + 2.0f, y, 3.0f, 1.0f);
        }
        g.setFont (st::fonts::medium (10.5f));
        g.setColour (grHold < -0.05f ? style::grColour() : t.text.withAlpha (0.85f));
        g.drawText (juce::String (grHold, 1), readout.withX (cx - colW * 0.5f).withWidth (colW), juce::Justification::centred, false);
        st::paint::caption (g, "GR", caption.withX (cx - colW * 0.5f).withWidth (colW), t.textFaint, juce::Justification::centred, 9.5f);
    }
}

} // namespace stdc
