#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FactoryPresets.h"
#include "ui/Strip.h"
#include "ui/Style.h"

using namespace stdc;

namespace
{
    constexpr int kChunk = 64;   // parameter update granularity (samples)
}

void StDcProcessor::Refs::bind (juce::AudioProcessorValueTreeState& s)
{
    role.bind (s, ids::role);               engine.bind (s, ids::engine);
    inGain.bind (s, ids::inputGain);        threshold.bind (s, ids::threshold);
    ratio.bind (s, ids::ratio);             knee.bind (s, ids::knee);
    range.bind (s, ids::range);             attack.bind (s, ids::attack);
    release.bind (s, ids::release);         autoRelease.bind (s, ids::autoRelease);
    drive.bind (s, ids::drive);             character.bind (s, ids::character);
    mix.bind (s, ids::mix);                 outGain.bind (s, ids::outputGain);
    autoGain.bind (s, ids::autoGain);       scHpf.bind (s, ids::scHighPass);
    link.bind (s, ids::stereoLink);         scExt.bind (s, ids::scExternal);
    quality.bind (s, ids::quality);         bypass.bind (s, ids::bypass);
}

StDcProcessor::StDcProcessor()
    : ProcessorBase (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                                      .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false),
                     createParameterLayout())
{
    refs.bind (apvts);
    registerFactoryPresets (presets);
    stdc::style::applyTheme();   // dark brown identity
    st::Knob::setDoubleClickEntersValue (true);   // Pro-style: double-click types a value, Cmd-click resets
    apvts.addParameterListener (ids::quality, this);
}

StDcProcessor::~StDcProcessor()
{
    unregisterInstance();   // first: views of this instance (elsewhere) still use its members
    apvts.removeParameterListener (ids::quality, this);
    cancelPendingUpdate();
}

bool StDcProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    if (in != out) return false;
    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

void StDcProcessor::applyQualityChange()
{
    const int q = juce::jlimit (0, 2, refs.quality.getInt());
    if (! prepared || q == engine.getQuality()) return;
    // re-preparing the colour stage allocates: never on the audio thread
    suspendProcessing (true);
    engine.setQuality (q);
    suspendProcessing (false);
    setLatencySamples (engine.getLatencySamples());
}

stdc::CompSettings StDcProcessor::getCompSettings() const noexcept
{
    CompSettings c;
    c.algorithm = getAlgorithm();
    c.thresholdDb = refs.threshold.get();
    c.ratio = refs.ratio.get();
    c.kneeDb = refs.knee.get();
    c.rangeDb = refs.range.get();
    c.attackMs = refs.attack.get();
    c.releaseMs = refs.release.get();
    c.autoRelease = refs.autoRelease.getBool();
    c.stereoLink = refs.link.get() * 0.01f;
    return c;
}

void StDcProcessor::updateEngineSettings() noexcept
{
    EngineSettings s;
    s.comp = getCompSettings();
    s.inputDb = refs.inGain.get();
    s.outputDb = refs.outGain.get();
    s.autoGain = refs.autoGain.getBool();
    s.drive = refs.drive.get() * 0.01f;
    s.character = juce::jlimit (0, 2, refs.character.getInt());
    s.mix = refs.mix.get() * 0.01f;
    s.scHighPassHz = refs.scHpf.get();
    s.externalSidechain = refs.scExt.getBool();
    s.bypass = refs.bypass.getBool();
    engine.setSettings (s);
}

void StDcProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    prepareShell (sampleRate);
    updateEngineSettings();
    engine.setQuality (juce::jlimit (0, 2, refs.quality.getInt()));
    engine.prepare (sampleRate, kChunk, juce::jlimit (1, 2, getMainBusNumOutputChannels()));
    prepared = true;
    setLatencySamples (engine.getLatencySamples());
    juce::ignoreUnused (samplesPerBlock);
}

void StDcProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int total = buffer.getNumSamples();

    auto main = getBusBuffer (buffer, false, 0);
    for (int c = getMainBusNumInputChannels(); c < main.getNumChannels(); ++c) main.clear (c, 0, total);
    const int channels = juce::jmin (2, main.getNumChannels());
    if (channels == 0) return;

    // (a buffer that refers to the host's channels: no copy, no allocation)
    const bool hasSc = getBusCount (true) > 1 && getChannelCountOfBus (true, 1) > 0;
    auto sc = hasSc ? getBusBuffer (buffer, true, 1) : juce::AudioBuffer<float>();

    inputMeter.push (main.getArrayOfReadPointers(), channels, total);
    preAnalyzer.push (main.getArrayOfReadPointers(), channels, total);

    for (int start = 0; start < total; start += kChunk)
    {
        const int n = juce::jmin (kChunk, total - start);
        float* ptrs[2] = { main.getWritePointer (0, start), channels > 1 ? main.getWritePointer (1, start) : nullptr };
        juce::AudioBuffer<float> chunk (ptrs, channels, n);

        juce::AudioBuffer<float> scChunk;
        if (hasSc)
        {
            float* sp[2] = { sc.getWritePointer (0, start), sc.getNumChannels() > 1 ? sc.getWritePointer (1, start) : nullptr };
            scChunk.setDataToReferTo (sp, juce::jmin (2, sc.getNumChannels()), n);
        }

        updateEngineSettings();
        engine.process (chunk, hasSc ? &scChunk : nullptr, n);
    }

    outputMeter.push (main.getArrayOfReadPointers(), channels, total);
    postAnalyzer.push (main.getArrayOfReadPointers(), channels, total);
}

//==============================================================================
void StDcProcessor::selectRole (int role)
{
    role = juce::jlimit (0, kNumRoles - 1, role);
    undoManager.beginNewTransaction ("Drum part: " + juce::String (getRoleInfo (role).name));
    auto set = [this] (const juce::String& id, float plain)
    {
        if (auto* p = apvts.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (plain));
            p->endChangeGesture();
        }
    };
    set (ids::role, (float) role);
    set (ids::engine, 0.0f);
    for (auto& [id, value] : getRoleInfo (role).startingPoint) set (id, value);
    undoManager.beginNewTransaction();
}

std::unique_ptr<juce::Component> StDcProcessor::createStripView()
{
    return std::make_unique<stdc::DcStrip> (*this);
}

int StDcProcessor::getStripWidth (int height) const
{
    return stdc::DcStrip::widthFor (height);
}

juce::AudioProcessorEditor* StDcProcessor::createEditor()
{
    return new StDcEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new StDcProcessor();
}
