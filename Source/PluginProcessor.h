#pragma once
#include "Params.h"
#include "Roles.h"
#include "dsp/Engine.h"

class StDcProcessor : public st::ProcessorBase,
                      private juce::AudioProcessorValueTreeState::Listener,
                      private juce::AsyncUpdater
{
public:
    StDcProcessor();
    ~StDcProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    bool isBusesLayoutSupported (const BusesLayout&) const override;

    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;
    /** ST DC's module in the multiview (st::MultiView). */
    std::unique_ptr<juce::Component> createStripView() override;
    int getStripWidth (int height) const override;
    double getTailLengthSeconds() const override { return 0.0; }

    stdc::Engine& getEngine() noexcept { return engine; }
    const stdc::Engine& getEngine() const noexcept { return engine; }

    int getRole() const noexcept { return refs.role.getInt(); }
    /** The engine running now (from the drum part, or the explicit Engine choice). */
    stdc::Algorithm getAlgorithm() const noexcept { return stdc::resolveAlgorithm (refs.role.getInt(), refs.engine.getInt()); }
    /** Compressor settings as the parameters say now (for the knee display). */
    stdc::CompSettings getCompSettings() const noexcept;

    /** Picks a drum part from the UI: switches the engine and moves the controls to that part's starting point,
        as one undoable step. (Automating the `role` parameter only switches the engine.) */
    void selectRole (int role);

    /** Applies a pending oversampling change now (normally done asynchronously on the message thread). */
    void applyQualityChange();

    juce::RangedAudioParameter& param (const juce::String& id) { return *apvts.getParameter (id); }

private:
    stdc::Engine engine;
    struct Refs
    {
        st::params::Ref role, engine, inGain, threshold, ratio, knee, range, attack, release, autoRelease,
                        drive, character, mix, outGain, autoGain, scHpf, link, scExt, quality, bypass;
        void bind (juce::AudioProcessorValueTreeState&);
    } refs;
    bool prepared = false;

    void updateEngineSettings() noexcept;
    void parameterChanged (const juce::String&, float) override { triggerAsyncUpdate(); }
    void handleAsyncUpdate() override { applyQualityChange(); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StDcProcessor)
};
