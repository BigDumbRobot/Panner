#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Bake.h"
#include "DriftPrompt.h"
#include "ProToolsBridge.h"
#include "SpatialPanner.h"

class PannerProcessor : public juce::AudioProcessor,
                        private juce::AudioProcessorValueTreeState::Listener,
                        private juce::ChangeListener,
                        private juce::AsyncUpdater
{
public:
    PannerProcessor();
    ~PannerProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                         { return true; }

    const juce::String getName() const override             { return JucePlugin_Name; }
    bool acceptsMidi() const override                       { return false; }
    bool producesMidi() const override                      { return false; }
    double getTailLengthSeconds() const override            { return 0.0; }

    int getNumPrograms() override                           { return 1; }
    int getCurrentProgram() override                        { return 0; }
    void setCurrentProgram (int) override                   {}
    const juce::String getProgramName (int) override        { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;
    void updateTrackProperties (const TrackProperties&) override;

    /** Parameter ID for one setting of a sequencer step: 'x', 'y', 'z', 'm' (move) or 'r' (radius). */
    static juce::String stepParamId (int step, char axis)
    {
        return "s" + juce::String (step) + juce::String::charToString ((juce::juce_wchar) axis);
    }

    // The three answers to a drift prompt, plus the push button. Message thread.
    void pushToProTools()                                   { bridge.requestPush(); }
    void revertToLastPush();

    /** True when committing would change Pro Tools' automation: nothing written yet, the
        steps changed since the last write, or the lanes were edited in Pro Tools. */
    bool differsFromWrittenAutomation() const;
    bool rendersObjects() const                             { return renderMode->load() < 0.5f; }
    void unlinkFromProTools()                               { bridge.unlink(); }

    /** Lets the processor move the drift prompt between the editor and its own window. */
    void editorOpenedOrClosed()                             { triggerAsyncUpdate(); }

    juce::AudioProcessorValueTreeState apvts;

    /** Replaces a step's hand-drawn path with a stroke in room coordinates. Empty clears it.
        Message thread. */
    void setDrawing (int step, const std::vector<juce::Point<float>>& stroke);

    /** The pattern as the parameters stand now. */
    Pattern readPattern() const;

    /** Where the source is right now, worked out from the last timeline position the host
        reported. Lets the editor animate smoothly between audio callbacks, and still show
        the right spot while the host has stopped calling the plug-in. step is -1 when the
        sequencer is off. */
    Position displayPosition (int& step) const;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameters();

    ProToolsBridge::Snapshot snapshotParameters() const;
    static Pattern patternFromSnapshot (const ProToolsBridge::Snapshot&);
    juce::String getTrackName() const;

    void parameterChanged (const juce::String&, float) override   { bridge.requestCheck(); }
    void changeListenerCallback (juce::ChangeBroadcaster*) override { updatePrompt(); }
    void handleAsyncUpdate() override                               { updatePrompt(); }
    void updatePrompt();

    static constexpr int maxInputs = 2;
    static constexpr int chunkSize = 32;

    using Param = std::atomic<float>*;
    Param x, y, z, size, width, gain, seqOn, seqSteps, seqRate, seqGlide, renderMode;
    std::array<std::array<Param, 5>, Sequencer::maxSteps> stepParams; // x, y, z, move, radius

    // Hand-drawn paths. Not parameters, so they're saved with the state and read under a lock.
    mutable juce::SpinLock drawingLock;
    std::array<Sequencer::Drawing, Sequencer::maxSteps> drawings {};
    std::array<bool, Sequencer::maxSteps> hasDrawing {};

    void setDrawingInternal (int step, const Sequencer::Drawing*);
    static juce::String drawingKey (int step)   { return "draw" + juce::String (step); }

    SpeakerLayout layout;
    double sampleRate = 44100.0;
    bool snapGains = true, wasPlaying = false;
    juce::int64 expectedSample = 0;

    // Last timeline the host reported, for displayPosition().
    std::atomic<double> hostPpq { 0.0 }, hostBpm { 120.0 }, hostStampMs { 0.0 };
    std::atomic<bool> hostPlaying { false };
    std::array<std::array<float, SpeakerLayout::maxChannels>, maxInputs> gains {};

    juce::CriticalSection trackNameLock;
    juce::String trackName;

public:
    ProToolsBridge bridge;

private:
    std::unique_ptr<DriftWindow> driftWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PannerProcessor)
};
