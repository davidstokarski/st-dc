// ST DC whole-plugin tests: engine curves and timing, nulls, latency, stress, state, the Drum Multiview across
// binaries, and editor snapshots (--snapshot <dir>).
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ui/Strip.h"
#include "ui/Display.h"
#if JUCE_MAC
 #include <AudioToolbox/AudioToolbox.h>
#endif

using namespace stdc;

namespace
{
int failures = 0;
void check (bool ok, const juce::String& what)
{
    if (! ok) { ++failures; std::cout << "FAIL: " << what << "\n"; }
}

void setParam (StDcProcessor& p, const juce::String& id, float plain)
{
    auto& prm = p.param (id);
    prm.setValueNotifyingHost (prm.convertTo0to1 (plain));
}

float runNoise (StDcProcessor& p, int blocks, int blockSize, bool& finite, int channels = 2, float level = 0.3f, float floor = 0.2f)
{
    juce::AudioBuffer<float> buf (channels, blockSize);
    static juce::Random rng (1);
    static int64_t clock = 0;   // keeps the hit pattern going across calls
    juce::MidiBuffer midi;
    float peak = 0.0f;
    for (int b = 0; b < blocks; ++b)
    {
        for (int c = 0; c < channels; ++c)
            for (int i = 0; i < blockSize; ++i)
            {
                const int n = (int) ((clock + b * blockSize + i) % 48000000);
                const float hit = (n % 12000) < 2400 ? std::exp (-(float) (n % 12000) / 500.0f) : 0.0f;   // drum-ish bursts
                buf.setSample (c, i, level * ((rng.nextFloat() * 2.0f - 1.0f) * (floor + hit) + 0.4f * std::sin ((float) n * 0.01f) * hit));
            }
        p.processBlock (buf, midi);
        juce::ignoreUnused (b);
        for (int c = 0; c < juce::jmin (2, channels); ++c)
            for (int i = 0; i < blockSize; ++i)
            {
                const float v = buf.getSample (c, i);
                if (! std::isfinite (v)) finite = false;
                peak = juce::jmax (peak, std::abs (v));
            }
    }
    clock += (int64_t) blocks * blockSize;
    return peak;
}

CompSettings baseSettings (Algorithm a)
{
    CompSettings s;
    s.algorithm = a;
    s.thresholdDb = -24.0f;
    s.ratio = 4.0f;
    s.kneeDb = 6.0f;
    s.rangeDb = 48.0f;
    s.attackMs = 5.0f;
    s.releaseMs = 100.0f;
    return s;
}

/** Runs the compressor on a constant-magnitude signal (square wave) and returns the settled gain (dB). */
float settledGain (Compressor& c, float levelDb, int samples, double sr = 48000.0)
{
    std::vector<float> det (64);
    const float a = st::dsp::dbToGain (levelDb);
    float g = 0.0f;
    int done = 0;
    while (done < samples)
    {
        const int n = juce::jmin (64, samples - done);
        for (int i = 0; i < n; ++i) det[(size_t) i] = ((done + i) & 1) ? a : -a;
        const float* ptrs[2] = { det.data(), det.data() };
        c.computeGain (ptrs, 2, n);
        g = c.getGainDb (0)[n - 1];
        done += n;
    }
    juce::ignoreUnused (sr);
    return g;
}

const char* algoName (Algorithm a) { return getAlgorithmInfo ((int) a).name; }
}

//==============================================================================
static void testCurves()
{
    std::cout << "Engines: steady state on the static curve\n";
    for (int ai = 0; ai < kNumAlgorithms; ++ai)
    {
        const auto a = (Algorithm) ai;
        for (bool autoRel : { false, true })
            for (float level : { -40.0f, -27.0f, -18.0f, -6.0f })
            {
                Compressor c;
                c.prepare (48000.0, 64);
                auto s = baseSettings (a);
                s.autoRelease = autoRel;
                c.setSettings (s);
                const float g = settledGain (c, level, 48000 * 3);
                const float want = Compressor::staticGainDb (level, s);
                check (std::abs (g - want) < 0.6f, juce::String (algoName (a)) + (autoRel ? " auto" : "") + " at " + juce::String (level)
                                                   + " dB: settled " + juce::String (g, 2) + " vs curve " + juce::String (want, 2));
            }

        // curve shape: no gain change well below the threshold, more reduction for louder input, ratio respected
        auto s = baseSettings (a);
        check (std::abs (Compressor::staticGainDb (-50.0f, s)) < 0.01f, juce::String (algoName (a)) + ": unity below threshold");
        const float g12 = Compressor::staticGainDb (-12.0f, s), g0 = Compressor::staticGainDb (0.0f, s);
        if (a == Algorithm::fetAll)
            check (g0 < g12 - 9.0f, "FET All over-compresses (louder input, more than 1:1 reduction)");
        else
        {
            check (g0 < g12, juce::String (algoName (a)) + ": more reduction for louder input");
            // classic engines: 4:1 above the knee = -0.75 dB per dB (opto / vari-mu approach it as the ratio rises)
            const float slope = (g0 - g12) / 12.0f;
            if (a == Algorithm::busVca || a == Algorithm::punchVca || a == Algorithm::fet)
                check (std::abs (slope + 0.75f) < 0.03f, juce::String (algoName (a)) + ": 4:1 slope (" + juce::String (slope, 3) + ")");
            else
                check (slope < -0.45f && slope > -0.9f, juce::String (algoName (a)) + ": programme-dependent slope (" + juce::String (slope, 3) + ")");
        }
        s.rangeDb = 6.0f;
        check (Compressor::staticGainDb (0.0f, s) > -6.01f && Compressor::staticGainDb (0.0f, s) < -5.0f, juce::String (algoName (a)) + ": range limits");
    }
}

static void testTiming()
{
    std::cout << "Engines: timing\n";
    auto timeTo = [] (Algorithm a, float attackMs, float fraction)
    {
        Compressor c;
        c.prepare (48000.0, 64);
        auto s = baseSettings (a);
        s.attackMs = attackMs;
        c.setSettings (s);
        const float target = Compressor::staticGainDb (-6.0f, s);
        std::vector<float> det (1, st::dsp::dbToGain (-6.0f));
        for (int i = 0; i < 48000; ++i)
        {
            const float* ptrs[2] = { det.data(), det.data() };
            det[0] = (i & 1) ? std::abs (det[0]) : -std::abs (det[0]);
            c.computeGain (ptrs, 2, 1);
            if (c.getGainDb (0)[0] <= target * fraction) return (float) i / 48.0f;
        }
        return 1000.0f;
    };
    const float fet = timeTo (Algorithm::fet, 0.05f, 0.8f), opto = timeTo (Algorithm::opto, 0.05f, 0.8f), bus = timeTo (Algorithm::busVca, 10.0f, 0.8f);
    check (fet < 1.0f, "FET grabs within 1 ms (" + juce::String (fet, 2) + " ms)");
    check (opto > 2.0f, "Opto can't be instant (" + juce::String (opto, 2) + " ms)");
    check (bus > 8.0f && bus < 40.0f, "Bus VCA 10 ms attack (" + juce::String (bus, 2) + " ms to 80%)");

    // Punch VCA: constant-rate release (release = time to recover 10 dB)
    {
        Compressor c;
        c.prepare (48000.0, 64);
        auto s = baseSettings (Algorithm::punchVca);
        s.releaseMs = 100.0f;
        s.ratio = 100.0f;
        c.setSettings (s);
        const float g0 = settledGain (c, -6.0f, 48000);
        const float g1 = settledGain (c, -80.0f, 4800);   // 100 ms later
        check (g0 < -15.0f && std::abs ((g1 - g0) - 10.0f) < 1.5f, "Punch VCA recovers ~10 dB per release time (" + juce::String (g1 - g0, 2) + " dB)");
    }

    // Auto release: a short hit recovers faster than sustained compression
    auto recovery = [] (int burstSamples)
    {
        Compressor c;
        c.prepare (48000.0, 64);
        auto s = baseSettings (Algorithm::busVca);
        s.autoRelease = true;
        s.attackMs = 1.0f;
        c.setSettings (s);
        settledGain (c, -6.0f, burstSamples);
        int n = 0;
        while (n < 48000 * 10)
        {
            if (settledGain (c, -80.0f, 64) > -1.0f) break;
            n += 64;
        }
        return (float) n / 48.0f;
    };
    const float shortHit = recovery (480), longHit = recovery (48000 * 3);
    check (longHit > shortHit * 1.5f, "Auto release: sustained compression recovers slower (" + juce::String (shortHit, 0) + " ms vs " + juce::String (longHit, 0) + " ms)");

    // Opto: the release tail lengthens after long compression
    auto optoRecovery = [] (int burstSamples)
    {
        Compressor c;
        c.prepare (48000.0, 64);
        auto s = baseSettings (Algorithm::opto);
        s.releaseMs = 400.0f;
        c.setSettings (s);
        settledGain (c, -6.0f, burstSamples);
        int n = 0;
        while (n < 48000 * 20) { if (settledGain (c, -80.0f, 64) > -1.0f) break; n += 64; }
        return (float) n / 48.0f;
    };
    const float optoShort = optoRecovery (4800), optoLong = optoRecovery (48000 * 5);
    check (optoLong > optoShort * 1.3f, "Opto memory: longer tail after long compression (" + juce::String (optoShort, 0) + " vs " + juce::String (optoLong, 0) + " ms)");
}

static void testNulls()
{
    std::cout << "Engine: bypass / mix nulls, latency alignment\n";
    for (int q = 0; q < 3; ++q)
        for (int mode = 0; mode < 2; ++mode)
        {
            Engine e;
            e.setQuality (q);
            e.prepare (48000.0, 64, 2);
            EngineSettings s;
            s.comp = baseSettings (Algorithm::fetAll);
            s.drive = 0.5f;
            s.bypass = mode == 0;
            s.mix = mode == 0 ? 1.0f : 0.0f;
            e.setSettings (s);
            e.reset();
            const int L = e.getLatencySamples();
            std::vector<float> in;
            juce::Random rng (3);
            float maxErr = 0.0f;
            juce::AudioBuffer<float> buf (2, 64);
            for (int b = 0; b < 200; ++b)
            {
                for (int i = 0; i < 64; ++i)
                {
                    const float v = (rng.nextFloat() * 2.0f - 1.0f) * 0.5f;
                    in.push_back (v);
                    buf.setSample (0, i, v);
                    buf.setSample (1, i, v);
                }
                e.process (buf, nullptr, 64);
                for (int i = 0; i < 64; ++i)
                {
                    const int idx = b * 64 + i - L;
                    if (b < 10 || idx < 0) continue;   // smoothers settle first
                    maxErr = juce::jmax (maxErr, std::abs (buf.getSample (0, i) - in[(size_t) idx]));
                }
            }
            check (maxErr < 1.0e-5f, juce::String (mode == 0 ? "Bypass" : "Mix 0%") + " nulls with the input delayed by the latency, oversampling "
                                     + juce::String (q) + " (err " + juce::String (maxErr, 8) + ", latency " + juce::String (L) + ")");
        }

    // make-up / auto gain sanity
    CompSettings s = baseSettings (Algorithm::busVca);
    check (Compressor::autoMakeupDb (s) > 2.0f && Compressor::autoMakeupDb (s) < 12.0f, "auto make-up in a sensible range (" + juce::String (Compressor::autoMakeupDb (s), 2) + " dB)");
}

static void testProcessor()
{
    std::cout << "Processor: stress, latency, roles, state\n";
    StDcProcessor p;
    p.setPlayConfigDetails (2, 2, 48000.0, 512);
    p.prepareToPlay (48000.0, 512);
    bool finite = true;

    for (int q = 0; q < 3; ++q)
    {
        setParam (p, ids::quality, (float) q);
        p.applyQualityChange();
        check (p.getLatencySamples() == p.getEngine().getLatencySamples(), "latency reported, oversampling " + juce::String (q));
        for (int r = 0; r < kNumRoles; ++r)
            for (int eng = 0; eng <= kNumAlgorithms; eng += 3)
            {
                p.selectRole (r);
                setParam (p, ids::engine, (float) eng);
                setParam (p, ids::drive, (float) ((r * 17 + q * 31) % 100));
                setParam (p, ids::character, (float) ((r + q) % 3));
                setParam (p, ids::inputGain, 12.0f);
                const float peak = runNoise (p, 12, 512, finite);
                check (peak < 8.0f, "bounded: q=" + juce::String (q) + " part=" + getRoleInfo (r).name + " engine=" + juce::String (eng) + " peak=" + juce::String (peak));
                check (p.getAlgorithm() == resolveAlgorithm (r, eng), "engine follows part / override");
            }
    }
    setParam (p, ids::inputGain, 0.0f);
    check (finite, "all output finite");

    for (int bs : { 1, 7, 63, 64, 65, 333, 2048 })
    {
        p.prepareToPlay (44100.0, bs);
        const float peak = runNoise (p, juce::jmax (4, 4096 / bs), bs, finite);
        check (finite && peak < 8.0f, "block size " + juce::String (bs));
    }
    p.prepareToPlay (48000.0, 512);

    // compression actually happens and gain reduction is metered
    p.selectRole ((int) Role::snare);
    runNoise (p, 40, 512, finite, 2, 0.9f);
    check (p.getEngine().grDb.load() < -1.0f || p.getEngine().history.getWritten() > 0, "gain reduction metered");
    {
        float deepest = 0.0f;
        const auto w = p.getEngine().history.getWritten();
        for (uint32_t i = w > 200 ? w - 200 : 0; i < w; ++i) deepest = juce::jmin (deepest, p.getEngine().history.read (i).grDb);
        check (deepest < -3.0f, "snare part compresses loud hits (" + juce::String (deepest, 1) + " dB)");
    }

    // picking a part loads its starting point, undo puts the old settings back
    setParam (p, ids::threshold, -7.0f);
    p.undoManager.beginNewTransaction();
    p.selectRole ((int) Role::cymbals);
    check (p.getRole() == (int) Role::cymbals && p.getAlgorithm() == Algorithm::opto, "cymbals run the opto engine");
    check (std::abs (p.apvts.getRawParameterValue (ids::threshold)->load() - (-22.0f)) < 0.05f, "cymbals starting point applied");

    // state round trip
    setParam (p, ids::ratio, 7.5f);
    setParam (p, ids::engine, 3.0f);
    juce::MemoryBlock state;
    p.getStateInformation (state);
    StDcProcessor p2;
    p2.setStateInformation (state.getData(), (int) state.getSize());
    check (std::abs (p2.apvts.getRawParameterValue (ids::ratio)->load() - 7.5f) < 0.05f, "state: ratio");
    check (p2.getRole() == (int) Role::cymbals && p2.getAlgorithm() == Algorithm::fet, "state: part + engine override");

    // presets load and run
    for (int i = 0; i < (int) p.presets.getPresets().size(); ++i)
    {
        p.presets.loadPreset (i);
        const float peak = runNoise (p, 6, 512, finite);
        check (finite && peak < 8.0f, "preset " + p.presets.getPresets()[(size_t) i].name);
    }

    // external side-chain: the detector follows the side-chain bus
    {
        StDcProcessor sc;
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add (juce::AudioChannelSet::stereo());
        layout.inputBuses.add (juce::AudioChannelSet::stereo());
        layout.outputBuses.add (juce::AudioChannelSet::stereo());
        check (sc.setBusesLayout (layout), "side-chain layout accepted");
        sc.setRateAndBufferSizeDetails (48000.0, 512);
        sc.prepareToPlay (48000.0, 512);
        setParam (sc, ids::scExternal, 1.0f);
        setParam (sc, ids::threshold, -30.0f);
        juce::AudioBuffer<float> buf (4, 512);
        juce::MidiBuffer midi;
        float deepest = 0.0f;
        for (int b = 0; b < 40; ++b)
        {
            for (int i = 0; i < 512; ++i)
            {
                buf.setSample (0, i, 0.01f * std::sin ((float) i * 0.1f));
                buf.setSample (1, i, 0.01f * std::sin ((float) i * 0.1f));
                buf.setSample (2, i, 0.8f * std::sin ((float) i * 0.05f));
                buf.setSample (3, i, 0.8f * std::sin ((float) i * 0.05f));
            }
            sc.processBlock (buf, midi);
            deepest = juce::jmin (deepest, sc.getEngine().grDb.load());
        }
        check (sc.getChannelCountOfBus (true, 1) > 0, "side-chain bus enabled");
        check (deepest < -6.0f, "a loud side-chain compresses a quiet main signal (" + juce::String (deepest, 1) + " dB)");
    }

    // CPU estimate: 4x oversampling, Room part (heaviest colour)
    {
        p.selectRole ((int) Role::room);
        setParam (p, ids::quality, 2.0f);
        p.applyQualityChange();
        const int blocks = 400;
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        runNoise (p, blocks, 512, finite);
        const auto ms = juce::Time::getMillisecondCounterHiRes() - t0;
        std::cout << "CPU (Room part, 4x, drive): " << juce::String (100.0 * ms / (blocks * 512 / 48.0), 2) << "% of one core\n";
    }
}

#if JUCE_MAC
namespace
{
    AudioUnit loadStAu (OSType subType)
    {
        AudioComponentDescription desc {};
        desc.componentType = kAudioUnitType_Effect;
        desc.componentSubType = subType;
        desc.componentManufacturer = 'StAu';
        AudioComponent comp = AudioComponentFindNext (nullptr, &desc);
        if (comp == nullptr) return nullptr;
        AudioUnit au = nullptr;
        if (AudioComponentInstanceNew (comp, &au) != noErr) return nullptr;
        return au;
    }
    void setAuTrack (AudioUnit au, const char* track)
    {
        CFStringRef name = juce::String (track).toCFString();
        AudioUnitSetProperty (au, kAudioUnitProperty_ContextName, kAudioUnitScope_Global, 0, &name, sizeof (name));
        CFRelease (name);
    }
}
#endif

/** The Drum Multiview: only tracks with an ST DC, every ST plug-in on them (other binaries too), in kit order. */
static void testMultiView()
{
    std::cout << "Drum Multiview\n";
    auto& reg = st::InstanceRegistry::get();
    auto kick = std::make_unique<StDcProcessor>();
    auto bus = std::make_unique<StDcProcessor>();
    kick->selectRole ((int) Role::kick);
    bus->selectRole ((int) Role::bus);
    juce::AudioProcessor::TrackProperties tk, tb;
    tk.name = juce::String ("Kick In");
    tb.name = juce::String ("Drums");
    kick->updateTrackProperties (tk);
    bus->updateTrackProperties (tb);

#if JUCE_MAC
    AudioUnit sat = loadStAu ('Stst');   // ST Sat on the kick track: shown
    AudioUnit eq = loadStAu ('Steq');    // ST EQ on a vocal track (no ST DC): not shown
    check (sat != nullptr && eq != nullptr, "ST Sat and ST EQ AUs installed and loaded in-process");
    if (sat != nullptr) setAuTrack (sat, "Kick In");
    if (eq != nullptr) setAuTrack (eq, "Vocal");
#endif
    reg.flush();

    std::unique_ptr<juce::AudioProcessorEditor> ed (kick->createEditor());
    ed->setSize (StDcEditor::designWidth, StDcEditor::designHeight);
    auto* e = dynamic_cast<StDcEditor*> (ed.get());
    e->openMultiView();
    auto* mv = e->getMultiView();
    mv->refresh();

    // other ST DC instances may exist in this process (none at this point in the tests)
    check (mv->getNumRows() == 2, "two drum tracks (" + juce::String (mv->getNumRows()) + " rows)");
    if (mv->getNumRows() >= 2)
    {
        const auto r0 = mv->getRowKinds (0), r1 = mv->getRowKinds (1);
        check (r0.size() == 1 && r0[0] == "ST DC", "Drum Bus row first (kit order): " + r0.joinIntoString (", "));
#if JUCE_MAC
        check (r1.size() == 2 && r1[0] == "ST DC" && r1[1] == "ST Sat", "Kick row: ST DC with the track's ST Sat: " + r1.joinIntoString (", "));
#endif
    }
    bool eqShown = false;
    for (int r = 0; r < mv->getNumRows(); ++r) eqShown |= mv->getRowKinds (r).contains ("ST EQ");
    check (! eqShown, "tracks without ST DC are not shown");

    auto* strip = dynamic_cast<DcStrip*> (mv->getModuleComponent (bus->getInstanceUid()));
    check (strip != nullptr && strip->getNumKnobs() == 6, "ST DC module: six knobs");
    if (strip != nullptr)
    {
        auto* k = strip->getKnob (0);
        const float before = bus->param (ids::threshold).getValue();
        auto src = juce::Desktop::getInstance().getMainMouseSource();
        const auto c = k->getLocalBounds().getCentre().toFloat();
        auto ev = [&] (juce::Point<float> pos, juce::ModifierKeys m, bool dragged)
        {
            const auto now = juce::Time::getCurrentTime();
            return juce::MouseEvent (src, pos, m, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, k, k, now, c, now, 1, dragged);
        };
        const juce::ModifierKeys lb (juce::ModifierKeys::leftButtonModifier);
        k->mouseDown (ev (c, lb, false));
        for (int i = 1; i <= 10; ++i) k->mouseDrag (ev (c.translated (0.0f, -6.0f * (float) i), lb, true));
        k->mouseUp (ev (c.translated (0.0f, -60.0f), {}, true));
        check (bus->param (ids::threshold).getValue() > before + 0.05f, "editing the module edits that instance");
    }

    // a part changes: the rows re-sort (snare after kick); a removed instance leaves
    {
        juce::Image img (juce::Image::ARGB, mv->getWidth(), mv->getHeight(), true);
        juce::Graphics g (img);
        mv->paintEntireComponent (g, true);
    }
    bus.reset();
    reg.flush();
    check (mv->getNumRows() == 1, "removed instance leaves the Drum Multiview");
#if JUCE_MAC
    if (sat != nullptr) AudioComponentInstanceDispose (sat);
    if (eq != nullptr) AudioComponentInstanceDispose (eq);
    reg.flush();
#endif
    ed.reset();
}

/** The spectrum: continuous reads of the analyser history (no skipped / repeated samples), and no flicker while
    audio arrives in big blocks between UI frames. */
static void testAnalyzer()
{
    std::cout << "Spectrum analyser\n";
    st::AnalyzerSource src;
    src.prepare (48000.0);
    st::SpectrumAnalyzer::Options o;
    o.fftOrder = 12; o.hopSize = 512; o.numDisplayPoints = 200; o.minHz = 20.0f; o.maxHz = 20000.0f;
    o.slopeDbPerOct = 0.0f; o.nonConsuming = true; o.smoothingPasses = 0; o.attack = 1.0f;
    st::SpectrumAnalyzer an (src, o);
    std::vector<float> block (2048);
    double phase = 0.0;
    auto pushSine = [&] (int n)
    {
        for (int i = 0; i < n; ++i) { block[(size_t) i] = 0.5f * (float) std::sin (phase); phase += 2.0 * juce::MathConstants<double>::pi * 1000.0 / 48000.0; }
        const float* ch[1] = { block.data() };
        src.push (ch, 1, n);
    };
    // big host blocks (2048) with 60 fps UI updates in between: the sine must stay clean (continuous reads)
    for (int b = 0; b < 40; ++b) { pushSine (2048); an.update (1.0 / 60.0); }
    float peak = -200.0f, far = -200.0f;
    for (int i = 0; i < an.getNumPoints(); ++i)
    {
        const float f = an.getFrequencyForPoint (i), l = an.getLevelsDb()[(size_t) i];
        if (std::abs (f - 1000.0f) < 60.0f) peak = juce::jmax (peak, l);
        if (f > 4000.0f) far = juce::jmax (far, l);
    }
    check (peak > -12.0f && far < peak - 70.0f, "clean sine in the spectrum (peak " + juce::String (peak, 1) + ", far " + juce::String (far, 1) + " dB)");
    // a UI frame without new audio (between host blocks) holds the display instead of dipping
    const auto before = an.getLevelsDb();
    const bool changed = an.update (1.0 / 60.0);
    check (! changed && an.getLevelsDb() == before, "no flicker between host blocks");
    // audio stopped: after a moment the display decays
    for (int i = 0; i < 30; ++i) an.update (1.0 / 60.0);
    float after = -200.0f;
    for (int i = 0; i < an.getNumPoints(); ++i) if (std::abs (an.getFrequencyForPoint (i) - 1000.0f) < 60.0f) after = juce::jmax (after, an.getLevelsDb()[(size_t) i]);
    check (after < peak - 3.0f, "decays when audio stops");
}

/** The level display: the scroll glides at a steady speed although audio arrives in host-block bursts, and the level
    line is a calm envelope instead of raw 4 ms peaks. */
static void testHistoryMotion()
{
    std::cout << "Level display motion\n";
    LevelHistory hist;
    hist.setFrameRate (250.0);
    HistoryReader raw (hist, 2048, 0.0f), smooth (hist, 2048, 260.0f);
    SmoothScroll scroll;
    juce::Random rng (7);
    double audioClock = 0.0, uiClock = 0.0, framesDue = 0.0;
    const double block = 1024.0 / 48000.0;   // host block: 1024 samples = 5.33 frames at a time
    std::vector<double> steps;
    double lastPos = 0.0, maxLag = 0.0;
    for (int ui = 0; ui < 600; ++ui)
    {
        uiClock += 1.0 / 60.0;
        while (audioClock + block <= uiClock)
        {
            audioClock += block;
            framesDue += block * 250.0;
            while (framesDue >= 1.0)
            {
                framesDue -= 1.0;
                hist.push ({ -20.0f + 12.0f * (rng.nextFloat() - 0.5f), -18.0f + 12.0f * (rng.nextFloat() - 0.5f), -3.0f, -20.0f });
            }
        }
        raw.update();
        smooth.update();
        const double pos = scroll.update (1.0 / 60.0, smooth.end(), 250.0);
        if (ui > 120) { steps.push_back (pos - lastPos); maxLag = juce::jmax (maxLag, (double) smooth.end() - pos); }
        lastPos = pos;
    }
    double mean = 0.0, worst = 0.0;
    for (auto d : steps) mean += d;
    mean /= (double) steps.size();
    for (auto d : steps) worst = juce::jmax (worst, std::abs (d - mean));
    std::cout << "  scroll: " << mean << " frames/UI frame, worst deviation " << worst << ", max lag " << maxLag << "\n";
    check (std::abs (mean - 250.0 / 60.0) < 0.1, "scroll speed = frame rate (" + juce::String (mean, 3) + " frames per UI frame)");
    check (worst < 0.6, "scroll glides evenly despite block bursts (worst deviation " + juce::String (worst, 3) + " frames)");
    check (maxLag < 40.0, "display stays close behind the audio (" + juce::String (maxLag, 1) + " frames)");

    auto roughness = [] (const HistoryReader& r)
    {
        double sum = 0.0;
        for (int i = 1; i < 400; ++i) sum += std::abs (r.get (i).outDb - r.get (i - 1).outDb);
        return sum / 399.0;
    };
    const double rr = roughness (raw), rs = roughness (smooth);
    std::cout << "  roughness: " << rr << " -> " << rs << " dB per frame\n";
    check (rs < rr * 0.45, "level line is an envelope, not raw peaks (" + juce::String (rr, 2) + " -> " + juce::String (rs, 2) + " dB per frame)");
}

static int runTests()
{
    juce::ScopedJuceInitialiser_GUI gui;
    testAnalyzer();
    testHistoryMotion();
    testCurves();
    testTiming();
    testNulls();
    testProcessor();
    testMultiView();
    std::cout << (failures == 0 ? "ALL PASSED" : juce::String (failures) + " FAILURES") << "\n";
    return failures == 0 ? 0 : 1;
}

//==============================================================================
namespace
{
/** Renders the editor after letting its timers run for a while (needs a running message loop). */
struct Snapshotter : juce::Timer
{
    struct Job { std::function<void()> setup; juce::String name; float scale = 1.0f;
                 std::function<void (juce::Component&)> afterOpen; int ticks = 70; };
    StDcProcessor& p;
    juce::File dir;
    std::vector<Job> jobs;
    size_t index = 0;
    int ticks = 0;
    bool opened = false;
    std::unique_ptr<juce::AudioProcessorEditor> ed;

    Snapshotter (StDcProcessor& proc, juce::File d) : p (proc), dir (d) {}

    void timerCallback() override
    {
        if (ed == nullptr)
        {
            if (index >= jobs.size()) { stopTimer(); juce::JUCEApplicationBase::quit(); return; }
            if (jobs[index].setup) jobs[index].setup();
            bool f = true;
            runNoise (p, 220, 1024, f, 2, 0.9f, 0.015f);   // ~4.7 s of hits, so the level history is full
            ed.reset (p.createEditor());
            ed->setSize (juce::roundToInt ((float) StDcEditor::designWidth * jobs[index].scale),
                         juce::roundToInt ((float) StDcEditor::designHeight * jobs[index].scale));
            ticks = 0;
            opened = false;
        }

        bool f = true;
        runNoise (p, 4, 256, f, 2, 0.9f, 0.015f);   // drum-like hits over a low floor

        if (! opened && ticks > 6 && jobs[index].afterOpen) { jobs[index].afterOpen (*ed); opened = true; }

        if (++ticks > jobs[index].ticks)
        {
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
            auto file = dir.getChildFile (jobs[index].name + ".png");
            file.deleteFile();
            juce::FileOutputStream out (file);
            juce::PNGImageFormat().writeImageToStream (img, out);
            std::cout << "wrote " << file.getFullPathName() << "\n";
            ed.reset();
            ++index;
        }
    }
};

juce::File snapshotDir;

struct SnapshotApp : juce::JUCEApplicationBase
{
    std::unique_ptr<StDcProcessor> proc;
    std::vector<std::unique_ptr<StDcProcessor>> others;
    std::unique_ptr<Snapshotter> snap;
#if JUCE_MAC
    std::vector<AudioUnit> units;
#endif

    const juce::String getApplicationName() override { return "StDcSnapshots"; }
    const juce::String getApplicationVersion() override { return "1"; }
    bool moreThanOneInstanceAllowed() override { return true; }
    void anotherInstanceStarted (const juce::String&) override {}
    void suspended() override {}
    void resumed() override {}
    void systemRequestedQuit() override { quit(); }
    void unhandledException (const std::exception*, const juce::String&, int) override {}
    void shutdown() override
    {
        snap.reset();
#if JUCE_MAC
        for (auto u : units) AudioComponentInstanceDispose (u);
#endif
        others.clear();
        proc.reset();
    }

    void initialise (const juce::String&) override
    {
        proc = std::make_unique<StDcProcessor>();
        auto& p = *proc;
        p.setPlayConfigDetails (2, 2, 48000.0, 512);
        p.prepareToPlay (48000.0, 512);
        snapshotDir.createDirectory();

        snap = std::make_unique<Snapshotter> (p, snapshotDir);
        snap->jobs.push_back ({ [&p] { p.selectRole ((int) Role::snare); }, "01_snare_fet", 1.0f, {} });
        snap->jobs.push_back ({ [&p] { p.selectRole ((int) Role::bus); }, "02_bus_vca", 1.0f, {} });
        snap->jobs.push_back ({ [&p] { p.selectRole ((int) Role::room); setParam (p, ids::quality, 1.0f); p.applyQualityChange(); }, "03_room_fetall", 1.0f, {} });
        snap->jobs.push_back ({ [&p] { p.selectRole ((int) Role::cymbals); setParam (p, ids::engine, 6.0f); }, "04_cymbals_override", 1.0f, {} });
        snap->jobs.push_back ({ [&p] { p.selectRole ((int) Role::kick); }, "05_kick_small", 0.75f, {} });

        // the Drum Multiview with a kit: bus, kick (+ ST EQ, ST Sat), snare, toms, room; a vocal track is left out
        snap->jobs.push_back ({ [this, &p]
        {
            juce::AudioProcessor::TrackProperties tp;
            tp.name = juce::String ("Kick In");
            p.updateTrackProperties (tp);
            p.selectRole ((int) Role::kick);
            const std::pair<const char*, Role> kit[] = { { "Drums", Role::bus }, { "Snare Top", Role::snare }, { "Tom 1", Role::tom },
                                                         { "Overheads", Role::cymbals }, { "Room", Role::room } };
            for (auto& [name, role] : kit)
            {
                auto o = std::make_unique<StDcProcessor>();
                o->setPlayConfigDetails (2, 2, 48000.0, 512);
                o->prepareToPlay (48000.0, 512);
                o->selectRole ((int) role);
                juce::AudioProcessor::TrackProperties t;
                t.name = juce::String (name);
                o->updateTrackProperties (t);
                bool f = true;
                runNoise (*o, 300, 512, f, 2, 0.9f, 0.015f);
                others.push_back (std::move (o));
            }
#if JUCE_MAC
            for (auto [sub, track] : { std::pair<OSType, const char*> { 'Steq', "Kick In" }, { 'Stst', "Kick In" }, { 'Stst', "Snare Top" }, { 'Steq', "Vocal" } })
                if (auto au = loadStAu (sub)) { setAuTrack (au, track); units.push_back (au); }
#endif
            st::InstanceRegistry::get().flush();
        }, "06_drum_multiview", 1.0f, [] (juce::Component& ed)
        {
            if (auto* e = dynamic_cast<StDcEditor*> (&ed)) { e->openMultiView(); e->getMultiView()->refresh(); }
        }, 90 });
        snap->startTimerHz (60);
    }
};

juce::JUCEApplicationBase* createSnapshotApp() { return new SnapshotApp(); }
}

int main (int argc, char** argv)
{
    juce::StringArray args (argv + 1, argc - 1);
    if (args.contains ("--snapshot"))
    {
        snapshotDir = juce::File (args[args.indexOf ("--snapshot") + 1]);
        juce::JUCEApplicationBase::createInstance = &createSnapshotApp;
        return juce::JUCEApplicationBase::main (argc, (const char**) argv);
    }
    return runTests();
}
