#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

/** Neon-on-dusk styling for every stock control in the editor. */
class VaporLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VaporLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float minPos, float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getLabelFont (juce::Label&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;

    /** The height control: a night sky between a floor and a roof line, pos in pixels. */
    static void drawHeightSky (juce::Graphics&, juce::Rectangle<float>, float pos);
};

/** One preset movement shown as a drawn icon; lit when it's the selected step's move. */
class MoveButton : public juce::Button
{
public:
    explicit MoveButton (int moveIndex);
    void paintButton (juce::Graphics&, bool over, bool down) override;

private:
    int move;
};

/** Thin bar under the step buttons. Its length is the pattern length; click or drag to set it. */
class LengthBar : public juce::Component,
                  public juce::SettableTooltipClient
{
public:
    std::function<int()> getSteps;
    std::function<void (int)> setSteps;
    std::function<void (bool)> gesture; // true on press, false on release

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    int stepAt (float x) const;
};

class PannerEditor : public juce::AudioProcessorEditor,
                     private juce::Timer,
                     private juce::ChangeListener
{
public:
    explicit PannerEditor (PannerProcessor&);
    ~PannerEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    /** The room, seen from above, isometrically, or in perspective from an orbiting camera.
        Dragging moves whichever target is selected, across the horizontal plane at its height.
        In the 3D view, Alt-drag or right-drag orbits the camera. */
    class XYPad : public juce::Component
    {
    public:
        enum class View { top, iso, orbit };

        explicit XYPad (PannerEditor& e) : owner (e) {}

        void paint (juce::Graphics&) override;
        void resized() override               { updateCamera(); }
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;

        /** Moves the source marker; called every frame. */
        void setLive (Position, int step);

        void setView (View);
        View getView() const                  { return view; }

    private:
        /** Room height relative to its width (2 units across). */
        static constexpr float roomHeight = 0.75f;
        static constexpr float cameraDistance = 4.6f;

        struct Camera
        {
            float yaw = 0.0f, pitch = juce::MathConstants<float>::halfPi;
            bool perspective = false;
            float scale = 1.0f;
            juce::Point<float> origin;
        };

        juce::Rectangle<float> room() const   { return getLocalBounds().toFloat().reduced (24.0f); }
        void updateCamera();
        juce::Point<float> projectRaw (float x, float y, float z, float* depthScale = nullptr) const;

        /** Room coordinates (x, y in -1..1, z 0 floor .. 1 ceiling) to screen. */
        juce::Point<float> project (float x, float y, float z) const;
        juce::Point<float> project (const Position& p) const   { return project (p.x, p.y, p.z); }
        float sizeAt (const Position&) const; // perspective size factor, 1 without perspective

        /** Screen to room x / y on the horizontal plane at height z. */
        juce::Point<float> toRoom (juce::Point<float> screen, float z) const;
        bool drawing() const;
        float editHeight() const;

        /** The path one step's movement traces, in screen space. */
        juce::Path movementPath (const Pattern&, int step) const;

        PannerEditor& owner;
        View view = View::top;
        float orbitYaw = -0.35f, orbitPitch = 0.55f;
        Camera cam;
        bool orbiting = false;
        juce::Point<float> orbitStart;
        float yawStart = 0.0f, pitchStart = 0.0f;
        Position live;
        int liveStep = -1;
        std::array<Position, 18> trail {};
        size_t trailHead = 0;
        bool trailFilled = false;
        std::vector<juce::Point<float>> stroke; // room coordinates, while drawing
    };

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override  { refreshBridgeStatus(); }
    void refreshBridgeStatus();
    void refreshCommitButton();

    /** Speaker format the room overlay shows. */
    juce::AudioChannelSet overlayLayout() const;
    bool overlayFollowsTrack() const;
    void setUpSlider (juce::Slider&, juce::Label&, const juce::String& text, juce::Slider::SliderStyle);

    /** step < 0 selects the manual position; otherwise a sequencer step. */
    void selectTarget (int step);
    void setMove (int move);
    int selectedMove() const   { return selected < 0 ? -1 : juce::roundToInt (value (PannerProcessor::stepParamId (selected, 'm'))); }
    juce::String targetId (char axis) const;
    juce::RangedAudioParameter* targetParam (char axis) const   { return proc.apvts.getParameter (targetId (axis)); }
    float value (const juce::String& id) const                  { return proc.apvts.getRawParameterValue (id)->load(); }

    VaporLookAndFeel lookAndFeel;
    PannerProcessor& proc;
    int selected = -1;
    juce::String moveTitleText { "MOVE" };

    // Header logo, drawn as a sun: sunLogoRise of its height shows above the horizon.
    static constexpr int sunLogoHeight = 112;
    static constexpr float sunLogoRise = 0.72f;
    juce::Image sunLogo;

    // Layout regions drawn by paint().
    juce::Rectangle<int> header, controlsPanel, sequencerPanel, statusBar, patternTitle, rateTitle, moveTitle, moveHintLow, moveHintHigh;

    void stepRate (int delta);

    XYPad pad { *this };
    juce::Slider zSlider, sizeSlider, widthSlider, gainSlider, glideSlider, radiusSlider;
    juce::Label zLabel, sizeLabel, widthLabel, gainLabel, glideLabel, radiusLabel, moveHint;
    std::array<MoveButton, 6> moveButtons { MoveButton (0), MoveButton (1), MoveButton (2),
                                            MoveButton (3), MoveButton (4), MoveButton (5) };
    juce::TextButton clearButton { "CLEAR DRAWING" };
    juce::TextButton rateDown { "<" }, rateUp { ">" };
    juce::ComboBox rateBox, renderBox, overlayBox;
    std::array<juce::TextButton, 3> viewButtons;
    LengthBar lengthBar;
    juce::TooltipWindow tooltips { this, 500 };
    juce::Label bridgeStatus;
    juce::TextButton pushButton { "COMMIT AUTOMATION" };
    int commitCheckFrames = 0;
    DriftPrompt driftPrompt;
    juce::ToggleButton seqButton { "SEQUENCER" };
    juce::TextButton manualButton { "MANUAL" };
    std::array<juce::TextButton, Sequencer::maxSteps> stepButtons;

    std::unique_ptr<SliderAttachment> zAttachment, radiusAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> rateAttachment, renderAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> seqAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PannerEditor)
};
