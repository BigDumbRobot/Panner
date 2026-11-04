#include "PluginProcessor.h"
#include "PluginEditor.h"

PannerProcessor::PannerProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::create7point1point4(), true)),
      apvts (*this, nullptr, "state", createParameters()),
      bridge ({ [this] { return readPattern(); },
                [this] { return getTrackName(); },
                [this] { return snapshotParameters(); } })
{
    auto raw = [this] (const juce::String& id) { return apvts.getRawParameterValue (id); };

    x = raw ("x");
    y = raw ("y");
    z = raw ("z");
    size = raw ("size");
    width = raw ("width");
    gain = raw ("gain");
    seqOn = raw ("seqOn");
    seqSteps = raw ("seqSteps");
    seqRate = raw ("seqRate");
    seqGlide = raw ("seqGlide");
    renderMode = raw ("renderMode");

    for (int i = 0; i < Sequencer::maxSteps; ++i)
        stepParams[(size_t) i] = { raw (stepParamId (i, 'x')), raw (stepParamId (i, 'y')), raw (stepParamId (i, 'z')),
                                   raw (stepParamId (i, 'm')), raw (stepParamId (i, 'r')) };

    // Any change to Panner after a push gets checked against Pro Tools straight away.
    for (auto* p : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            apvts.addParameterListener (ranged->getParameterID(), this);

    bridge.addChangeListener (this);
    bridge.start();
}

PannerProcessor::~PannerProcessor()
{
    bridge.removeChangeListener (this);
    cancelPendingUpdate();
    driftWindow.reset();

    for (auto* p : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            apvts.removeParameterListener (ranged->getParameterID(), this);
}

juce::AudioProcessorValueTreeState::ParameterLayout PannerProcessor::createParameters()
{
    using Float = juce::AudioParameterFloat;
    using Range = juce::NormalisableRange<float>;

    auto id = [] (const juce::String& name) { return juce::ParameterID { name, 1 }; };

    juce::AudioProcessorValueTreeState::ParameterLayout params;

    // Readable values for the editor and the host's own displays.
    using Attributes = juce::AudioParameterFloatAttributes;
    const auto percent = Attributes()
        .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; })
        .withValueFromStringFunction ([] (const juce::String& t) { return t.getFloatValue() / 100.0f; });
    const auto coordinate = Attributes()
        .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 2); });
    const auto decibels = Attributes()
        .withStringFromValueFunction ([] (float v, int) { return v <= -60.0f ? juce::String ("-inf dB") : juce::String (v, 1) + " dB"; })
        .withValueFromStringFunction ([] (const juce::String& t) { return t.getFloatValue(); });

    params.add (std::make_unique<juce::AudioParameterChoice> (id ("renderMode"), "Render",
                                                              juce::StringArray { "Object (Pro Tools pans)", "Bed (Panner pans)" }, 0),
                std::make_unique<Float> (id ("x"), "X", Range (-1.0f, 1.0f), 0.0f, coordinate),
                std::make_unique<Float> (id ("y"), "Y", Range (-1.0f, 1.0f), 1.0f, coordinate),
                std::make_unique<Float> (id ("z"), "Z", Range (0.0f, 1.0f), 0.0f, percent),
                std::make_unique<Float> (id ("size"), "Size", Range (0.0f, 1.0f), 0.0f, percent),
                std::make_unique<Float> (id ("width"), "Stereo Width", Range (0.0f, 1.0f), 1.0f, percent),
                std::make_unique<Float> (id ("gain"), "Gain", Range (-60.0f, 12.0f, 0.1f), 0.0f, decibels),
                std::make_unique<juce::AudioParameterBool> (id ("seqOn"), "Sequencer", false),
                std::make_unique<juce::AudioParameterInt> (id ("seqSteps"), "Steps", 1, Sequencer::maxSteps, 4),
                std::make_unique<juce::AudioParameterChoice> (id ("seqRate"), "Rate", Sequencer::rateNames, 2),
                std::make_unique<Float> (id ("seqGlide"), "Glide", Range (0.0f, 1.0f), 0.0f, percent));

    // Default pattern: clockwise around the room corners.
    static constexpr float cornerX[] { -1.0f, 1.0f, 1.0f, -1.0f };
    static constexpr float cornerY[] { 1.0f, 1.0f, -1.0f, -1.0f };

    for (int i = 0; i < Sequencer::maxSteps; ++i)
    {
        const auto name = "Step " + juce::String (i + 1);

        params.add (std::make_unique<Float> (id (stepParamId (i, 'x')), name + " X", Range (-1.0f, 1.0f), cornerX[i % 4], coordinate),
                    std::make_unique<Float> (id (stepParamId (i, 'y')), name + " Y", Range (-1.0f, 1.0f), cornerY[i % 4], coordinate),
                    std::make_unique<Float> (id (stepParamId (i, 'z')), name + " Z", Range (0.0f, 1.0f), 0.0f, percent),
                    std::make_unique<juce::AudioParameterChoice> (id (stepParamId (i, 'm')), name + " Move", Sequencer::moveNames, 0),
                    std::make_unique<Float> (id (stepParamId (i, 'r')), name + " Radius", Range (0.05f, 1.0f), 0.3f, percent));
    }

    return params;
}

bool PannerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    using CS = juce::AudioChannelSet;

    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (in != CS::mono() && in != CS::stereo())
        return false;

    if (out == in)
        return true; // insert on a Pro Tools object track

    static const CS outputs[] { CS::stereo(),
                                CS::create5point1(),
                                CS::create7point1(),
                                CS::create5point1point2(),
                                CS::create5point1point4(),
                                CS::create7point1point2(),
                                CS::create7point1point4(),
                                CS::create7point1point6(),
                                CS::create9point1point6() };

    return std::find (std::begin (outputs), std::end (outputs), out) != std::end (outputs);
}

void PannerProcessor::prepareToPlay (double newSampleRate, int)
{
    sampleRate = newSampleRate;
    layout.set (getBusesLayout().getMainOutputChannelSet());
    snapGains = true;
}

Pattern PannerProcessor::patternFromSnapshot (const ProToolsBridge::Snapshot& snapshot)
{
    const std::map<juce::String, float> values (snapshot.begin(), snapshot.end());

    auto value = [&] (const juce::String& id)
    {
        const auto it = values.find (id);
        return it != values.end() ? it->second : 0.0f;
    };

    Pattern p;
    p.manual = { value ("x"), value ("y"), value ("z") };
    p.size = value ("size");
    p.width = value ("width");
    p.sequenced = value ("seqOn") > 0.5f;
    p.numSteps = juce::jlimit (1, Sequencer::maxSteps, juce::roundToInt (value ("seqSteps")));
    p.stepBeats = Sequencer::rateInBeats (juce::roundToInt (value ("seqRate")));
    p.glide = value ("seqGlide");

    for (int i = 0; i < p.numSteps; ++i)
    {
        auto& step = p.steps[(size_t) i];
        step.position = { value (stepParamId (i, 'x')), value (stepParamId (i, 'y')), value (stepParamId (i, 'z')) };
        step.move = juce::roundToInt (value (stepParamId (i, 'm')));
        step.radius = value (stepParamId (i, 'r'));
        step.hasDrawing = value (drawingKey (i)) > 0.5f;

        if (step.hasDrawing)
            for (int k = 0; k < Sequencer::drawingPoints; ++k)
                step.drawing[(size_t) k] = { value (drawingKey (i) + "_" + juce::String (k) + "x"),
                                             value (drawingKey (i) + "_" + juce::String (k) + "y") };
    }

    return p;
}

bool PannerProcessor::differsFromWrittenAutomation() const
{
    if (bridge.getReport().status == ProToolsBridge::Status::drifted)
        return true; // someone edited the lanes in Pro Tools

    const auto written = bridge.getRevertSnapshot();
    return written.empty() || ! samePattern (readPattern(), patternFromSnapshot (written));
}

Pattern PannerProcessor::readPattern() const
{
    Pattern p;
    p.manual = { x->load(), y->load(), z->load() };
    p.size = size->load();
    p.width = width->load();
    p.sequenced = seqOn->load() > 0.5f;
    p.numSteps = juce::jlimit (1, Sequencer::maxSteps, juce::roundToInt (seqSteps->load()));
    p.stepBeats = Sequencer::rateInBeats (juce::roundToInt (seqRate->load()));
    p.glide = seqGlide->load();

    for (size_t i = 0; i < (size_t) p.numSteps; ++i)
    {
        auto& step = p.steps[i];
        step.position = { stepParams[i][0]->load(), stepParams[i][1]->load(), stepParams[i][2]->load() };
        step.move = juce::roundToInt (stepParams[i][3]->load());
        step.radius = stepParams[i][4]->load();
    }

    const juce::SpinLock::ScopedLockType sl (drawingLock);

    for (size_t i = 0; i < (size_t) p.numSteps; ++i)
    {
        p.steps[i].hasDrawing = hasDrawing[i];

        if (hasDrawing[i])
            p.steps[i].drawing = drawings[i];
    }

    return p;
}

void PannerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (isNonRealtime() && bridge.isLinked())
        bridge.noteOfflineRender();

    const int numIn = juce::jmin (getTotalNumInputChannels(), maxInputs);
    const int numOut = juce::jmin (layout.getNumChannels(), buffer.getNumChannels());
    const int numSamples = buffer.getNumSamples();

    // The sequencer follows the host timeline rather than counting samples, so a bounce
    // from any start point (realtime or offline) lands on the same positions.
    double ppq = 0.0, ppqPerSample = 0.0;
    bool hasTimeline = false, playing = false;
    juce::int64 timelineSample = 0;
    bool hasSamples = false;

    if (auto* playHead = getPlayHead())
    {
        if (auto info = playHead->getPosition())
        {
            playing = info->getIsPlaying();

            if (auto samples = info->getTimeInSamples())
            {
                timelineSample = *samples;
                hasSamples = true;
            }

            if (auto reportedPpq = info->getPpqPosition())
            {
                ppq = *reportedPpq;
                hasTimeline = true;

                if (playing)
                    if (auto bpm = info->getBpm())
                        ppqPerSample = *bpm / (60.0 * sampleRate);
            }
        }
    }

    // On play or a jump in the timeline, start at the right position instead of fading in
    // from wherever the last pass stopped, so every pass over the same spot renders the same.
    if (playing && (! wasPlaying || ! hasSamples || timelineSample != expectedSample))
        snapGains = true;

    wasPlaying = playing;
    expectedSample = timelineSample + numSamples;

    hostPpq = ppq;
    hostBpm = ppqPerSample * 60.0 * sampleRate;
    hostPlaying = playing && ppqPerSample > 0.0;
    hostStampMs = juce::Time::getMillisecondCounterHiRes();

    auto pattern = readPattern();
    pattern.sequenced = pattern.sequenced && hasTimeline;

    if (renderMode->load() < 0.5f)
    {
        // Object mode: Pro Tools' own panner places the audio from the pushed lanes.
        for (int ch = numIn; ch < buffer.getNumChannels(); ++ch)
            buffer.clear (ch, 0, numSamples);

        return;
    }

    const float halfWidth = numIn == 2 ? pattern.width : 0.0f;
    const float outputGain = juce::Decibels::decibelsToGain (gain->load(), -60.0f);

    Position position = pattern.manual;
    float input[maxInputs][chunkSize];
    float target[SpeakerLayout::maxChannels];

    // Chunks sit on a fixed grid of timeline samples, so the host's buffer size doesn't
    // change where positions are sampled.
    const int firstChunk = hasSamples ? chunkSize - (int) (((timelineSample % chunkSize) + chunkSize) % chunkSize)
                                      : chunkSize;

    for (int start = 0, n = 0; start < numSamples; start += n)
    {
        // A block can end mid-chunk; that piece still aims at the position at the grid point.
        const int toGrid = start == 0 ? firstChunk : chunkSize;
        n = juce::jmin (toGrid, numSamples - start);
        position = pattern.at (ppq + (start + toGrid) * ppqPerSample);

        // Input and output share the buffer, so take a copy before overwriting it.
        for (int ch = 0; ch < numIn; ++ch)
            std::copy_n (buffer.getReadPointer (ch, start), n, input[ch]);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.clear (ch, start, n);

        // ~10 ms one-pole on the gains keeps hard steps and parameter jumps click-free.
        const float smoothing = snapGains ? 1.0f : 1.0f - std::exp (-(float) n / (0.010f * (float) sampleRate));

        for (int ch = 0; ch < numIn; ++ch)
        {
            auto source = position;
            source.x = juce::jlimit (-1.0f, 1.0f, source.x + (ch == 0 ? -halfWidth : halfWidth));
            layout.computeGains (source, pattern.size, target);

            auto& current = gains[(size_t) ch];

            for (int out = 0; out < numOut; ++out)
            {
                const float startGain = snapGains ? target[out] * outputGain : current[(size_t) out];
                const float endGain = startGain + (target[out] * outputGain - startGain) * smoothing;
                current[(size_t) out] = endGain;

                if (juce::exactlyEqual (startGain, 0.0f) && juce::exactlyEqual (endGain, 0.0f))
                    continue;

                buffer.addFromWithRamp (out, start, input[ch], n, startGain, endGain);
            }
        }

        snapGains = false;
    }
}

Position PannerProcessor::displayPosition (int& step) const
{
    auto pattern = readPattern();
    double ppq = hostPpq;

    if (hostPlaying)
    {
        // Glide forward from the last callback, but don't run away if the host went quiet.
        const double ageMs = juce::jlimit (0.0, 250.0, juce::Time::getMillisecondCounterHiRes() - hostStampMs.load());
        ppq += ageMs / 60000.0 * hostBpm.load();
    }

    step = pattern.sequenced ? Sequencer::stepAt (ppq, pattern.stepBeats, pattern.numSteps) : -1;
    return pattern.at (ppq);
}

juce::AudioProcessorEditor* PannerProcessor::createEditor()
{
    return new PannerEditor (*this);
}

//==============================================================================
ProToolsBridge::Snapshot PannerProcessor::snapshotParameters() const
{
    ProToolsBridge::Snapshot snapshot;

    for (auto* p : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            snapshot.push_back ({ ranged->getParameterID(), ranged->convertFrom0to1 (ranged->getValue()) });

    // Drawings too, so undoing back to a push restores them: drawN = 1, then drawN_Kx / drawN_Ky.
    const juce::SpinLock::ScopedLockType sl (drawingLock);

    for (int i = 0; i < Sequencer::maxSteps; ++i)
    {
        snapshot.push_back ({ drawingKey (i), hasDrawing[(size_t) i] ? 1.0f : 0.0f });

        if (hasDrawing[(size_t) i])
            for (int k = 0; k < Sequencer::drawingPoints; ++k)
            {
                const auto& pt = drawings[(size_t) i][(size_t) k];
                snapshot.push_back ({ drawingKey (i) + "_" + juce::String (k) + "x", pt.x });
                snapshot.push_back ({ drawingKey (i) + "_" + juce::String (k) + "y", pt.y });
            }
    }

    return snapshot;
}

void PannerProcessor::setDrawingInternal (int step, const Sequencer::Drawing* drawing)
{
    {
        const juce::SpinLock::ScopedLockType sl (drawingLock);
        hasDrawing[(size_t) step] = drawing != nullptr;

        if (drawing != nullptr)
            drawings[(size_t) step] = *drawing;
    }

    bridge.requestCheck();
}

void PannerProcessor::setDrawing (int step, const std::vector<juce::Point<float>>& stroke)
{
    if (! juce::isPositiveAndBelow (step, Sequencer::maxSteps))
        return;

    if (stroke.empty())
    {
        setDrawingInternal (step, nullptr);
        return;
    }

    const auto drawing = Sequencer::resample (stroke);
    setDrawingInternal (step, &drawing);
    updateHostDisplay (ChangeDetails().withNonParameterStateChanged (true));
}

void PannerProcessor::revertToLastPush()
{
    const auto snapshot = bridge.getRevertSnapshot();
    std::map<juce::String, float> values (snapshot.begin(), snapshot.end());

    for (int i = 0; i < Sequencer::maxSteps; ++i)
    {
        const auto key = drawingKey (i);

        if (values.count (key) == 0)
            continue;

        if (values[key] < 0.5f)
        {
            setDrawingInternal (i, nullptr);
            continue;
        }

        Sequencer::Drawing d {};

        for (int k = 0; k < Sequencer::drawingPoints; ++k)
            d[(size_t) k] = { values[key + "_" + juce::String (k) + "x"], values[key + "_" + juce::String (k) + "y"] };

        setDrawingInternal (i, &d);
    }

    for (auto& [id, value] : snapshot)
    {
        if (auto* p = apvts.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (value));
            p->endChangeGesture();
        }
    }

    bridge.requestCheck();
}

juce::String PannerProcessor::getTrackName() const
{
    const juce::ScopedLock sl (trackNameLock);
    return trackName;
}

void PannerProcessor::updateTrackProperties (const TrackProperties& properties)
{
    {
        const juce::ScopedLock sl (trackNameLock);
        trackName = properties.name.value_or (juce::String());
    }

    bridge.requestCheck();
}

void PannerProcessor::updatePrompt()
{
    const auto report = bridge.getReport();

    // With the editor open the prompt covers the editor; otherwise it gets its own window.
    if (! report.needsAnswer() || getActiveEditor() != nullptr)
    {
        driftWindow.reset();
        return;
    }

    if (driftWindow == nullptr)
    {
        auto* prompt = new DriftPrompt();
        prompt->onPush = [this] { pushToProTools(); };
        prompt->onRevert = [this] { revertToLastPush(); };
        prompt->onUnlink = [this] { unlinkFromProTools(); };
        driftWindow = std::make_unique<DriftWindow> (prompt);
    }

    driftWindow->getPrompt().setReport (report);
}

//==============================================================================
void PannerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.appendChild (bridge.saveState(), nullptr);

    juce::ValueTree drawingTree ("Drawings");
    {
        const juce::SpinLock::ScopedLockType sl (drawingLock);

        for (int i = 0; i < Sequencer::maxSteps; ++i)
        {
            if (! hasDrawing[(size_t) i])
                continue;

            juce::StringArray points;

            for (auto& pt : drawings[(size_t) i])
                points.add (juce::String (pt.x, 4) + "," + juce::String (pt.y, 4));

            drawingTree.appendChild (juce::ValueTree ("Drawing", { { "step", i }, { "points", points.joinIntoString (" ") } }), nullptr);
        }
    }
    state.appendChild (drawingTree, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void PannerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto state = juce::ValueTree::fromXml (*xml);
    const auto bridgeState = state.getChildWithName ("ProToolsBridge");
    state.removeChild (bridgeState, nullptr);

    const auto drawingTree = state.getChildWithName ("Drawings");
    state.removeChild (drawingTree, nullptr);

    for (int i = 0; i < Sequencer::maxSteps; ++i)
        setDrawingInternal (i, nullptr);

    for (const auto& child : drawingTree)
    {
        const int step = child.getProperty ("step", -1);
        const auto points = juce::StringArray::fromTokens (child.getProperty ("points").toString(), " ", "");

        if (! juce::isPositiveAndBelow (step, Sequencer::maxSteps) || points.size() != Sequencer::drawingPoints)
            continue;

        Sequencer::Drawing d {};

        for (int k = 0; k < Sequencer::drawingPoints; ++k)
            d[(size_t) k] = { points[k].upToFirstOccurrenceOf (",", false, false).getFloatValue(),
                              points[k].fromFirstOccurrenceOf (",", false, false).getFloatValue() };

        setDrawingInternal (step, &d);
    }

    apvts.replaceState (state);
    bridge.loadState (bridgeState);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PannerProcessor();
}
