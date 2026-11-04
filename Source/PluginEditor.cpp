#include "PluginEditor.h"
#include "BinaryData.h"

namespace vapor
{
    const juce::Colour night     { 0xff0b0420 };
    const juce::Colour dusk      { 0xff1c0a40 };
    const juce::Colour violet    { 0xff3b1272 };
    const juce::Colour panel     { 0xc0120828 };
    const juce::Colour edge      { 0xff4f2590 };
    const juce::Colour dim       { 0xff9a87cf };
    const juce::Colour ice       { 0xffdcf7ff };
    const juce::Colour pink      { 0xffff2bd6 };
    const juce::Colour hot       { 0xffff3d81 };
    const juce::Colour cyan      { 0xff2de2e6 };
    const juce::Colour orange    { 0xffff8a3d };
    const juce::Colour yellow    { 0xffffd23f };
    const juce::Colour good      { 0xff3dffa8 };
    const juce::Colour warning   { 0xffffb347 };

    static juce::Font font (float size, bool bold = true, float kerning = 0.06f)
    {
        return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain)).withExtraKerningFactor (kerning);
    }

    static juce::Font mono (float size)
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size, juce::Font::bold));
    }

    /** A line with a soft neon halo. */
    static void glowStroke (juce::Graphics& g, const juce::Path& p, juce::Colour c, float width, float glow = 1.0f)
    {
        for (auto [scale, alpha] : { std::pair { 7.0f, 0.06f }, { 4.0f, 0.10f }, { 2.2f, 0.18f } })
        {
            g.setColour (c.withMultipliedAlpha (alpha * glow));
            g.strokePath (p, juce::PathStrokeType (width * scale, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    static void glowDot (juce::Graphics& g, juce::Point<float> centre, float diameter, juce::Colour c)
    {
        juce::ColourGradient halo (c.withAlpha (0.55f), centre, c.withAlpha (0.0f), centre.translated (diameter * 1.6f, 0.0f), true);
        g.setGradientFill (halo);
        g.fillEllipse (juce::Rectangle<float> (diameter * 3.2f, diameter * 3.2f).withCentre (centre));
        g.setColour (c);
        g.fillEllipse (juce::Rectangle<float> (diameter, diameter).withCentre (centre));
        g.setColour (ice);
        g.fillEllipse (juce::Rectangle<float> (diameter * 0.45f, diameter * 0.45f).withCentre (centre));
    }

    static void neonPanel (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setColour (panel);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (edge.withAlpha (0.8f));
        g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);

        // Sunset strip along the top edge.
        auto strip = r.removeFromTop (2.0f).reduced (10.0f, 0.0f);
        g.setGradientFill (juce::ColourGradient (cyan, strip.getX(), 0, pink, strip.getRight(), 0, false));
        g.fillRect (strip);
    }

    static void sectionTitle (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> r)
    {
        g.setFont (font (11.0f, true, 0.25f));
        g.setColour (cyan.withAlpha (0.85f));
        g.drawText (text, r, juce::Justification::centredLeft);
    }
}

//==============================================================================
VaporLookAndFeel::VaporLookAndFeel()
{
    using namespace vapor;
    setColour (juce::Label::textColourId, dim);
    setColour (juce::Slider::textBoxTextColourId, ice);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, pink.withAlpha (0.4f));
    setColour (juce::ComboBox::textColourId, ice);
    setColour (juce::PopupMenu::backgroundColourId, dusk);
    setColour (juce::PopupMenu::textColourId, ice);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, pink);
    setColour (juce::PopupMenu::highlightedTextColourId, night);
    setColour (juce::TextButton::textColourOffId, ice);
    setColour (juce::TextButton::textColourOnId, night);
    setColour (juce::TextEditor::backgroundColourId, night);
    setColour (juce::TextEditor::textColourId, ice);
    setColour (juce::TextEditor::highlightColourId, pink.withAlpha (0.4f));
    setColour (juce::CaretComponent::caretColourId, pink);
    setColour (juce::TooltipWindow::backgroundColourId, dusk);
    setColour (juce::TooltipWindow::textColourId, ice);
}

void VaporLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, juce::Slider& s)
{
    using namespace vapor;
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (6.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float arcRadius = radius - 4.0f;

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (violet);
    g.strokePath (track, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (pos > 0.001f)
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, angle, true);
        const auto c = cyan.interpolatedWith (pink, pos);
        glowStroke (g, value, c, 3.0f, s.isEnabled() ? 1.0f : 0.3f);
    }

    // Disc
    const float disc = arcRadius - 7.0f;
    g.setGradientFill (juce::ColourGradient (violet.brighter (0.15f), centre.x, centre.y - disc,
                                             night, centre.x, centre.y + disc, false));
    g.fillEllipse (juce::Rectangle<float> (disc * 2.0f, disc * 2.0f).withCentre (centre));
    g.setColour (edge);
    g.drawEllipse (juce::Rectangle<float> (disc * 2.0f, disc * 2.0f).withCentre (centre), 1.0f);

    // Pointer
    const auto tip = centre.getPointOnCircumference (disc - 4.0f, angle);
    const auto base = centre.getPointOnCircumference (disc * 0.35f, angle);
    g.setColour (ice);
    g.drawLine ({ base, tip }, 2.5f);
}

void VaporLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float, float, juce::Slider::SliderStyle style, juce::Slider& s)
{
    using namespace vapor;
    const auto r = juce::Rectangle<int> (x, y, w, h).toFloat();

    if (style == juce::Slider::LinearVertical)
    {
        drawHeightSky (g, r, pos);
        return;
    }

    // Horizontal: a thin line and a plain thumb, no glow.
    const float thickness = 3.0f;
    const auto track = juce::Rectangle<float> (r.getWidth(), thickness).withCentre (r.getCentre());
    g.setColour (violet);
    g.fillRoundedRectangle (track, thickness * 0.5f);

    const float alpha = s.isEnabled() ? 1.0f : 0.4f;
    g.setColour (cyan.withMultipliedAlpha (0.85f * alpha));
    g.fillRoundedRectangle (track.withRight (pos), thickness * 0.5f);

    const auto thumb = juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ pos, r.getCentreY() });
    g.setColour (ice.withMultipliedAlpha (alpha));
    g.fillEllipse (thumb);
    g.setColour (cyan.withMultipliedAlpha (alpha));
    g.drawEllipse (thumb, 1.5f);
}

void VaporLookAndFeel::drawHeightSky (juce::Graphics& g, juce::Rectangle<float> r, float pos)
{
    using namespace vapor;
    const auto sky = r.reduced (2.0f, 0.0f);

    // Night sky over a floor: deep blue-violet at the ceiling fading to dusk at the floor.
    juce::ColourGradient air (juce::Colour (0xff070216), 0.0f, sky.getY(), violet.darker (0.2f), 0.0f, sky.getBottom(), false);
    air.addColour (0.75, dusk);
    g.setGradientFill (air);
    g.fillRoundedRectangle (sky, 6.0f);

    // Stars, thinning towards the floor. Fixed positions so they don't twinkle on repaint.
    juce::Random random (7);

    for (int i = 0; i < 26; ++i)
    {
        const float fy = std::pow (random.nextFloat(), 1.8f) * 0.85f;
        const auto star = juce::Point<float> (sky.getX() + 4.0f + random.nextFloat() * (sky.getWidth() - 8.0f),
                                              sky.getY() + 16.0f + fy * (sky.getHeight() - 16.0f));
        const float size = random.nextFloat() < 0.15f ? 2.2f : 1.2f;
        g.setColour (ice.withAlpha (0.25f + 0.5f * (1.0f - fy)));
        g.fillEllipse (juce::Rectangle<float> (size, size).withCentre (star));
    }

    // Ceiling: a roof line across the top.
    const float ceiling = sky.getY() + 12.0f;
    juce::Path roof;
    roof.startNewSubPath (sky.getX() + 3.0f, ceiling + 5.0f);
    roof.lineTo (sky.getCentreX(), ceiling - 4.0f);
    roof.lineTo (sky.getRight() - 3.0f, ceiling + 5.0f);
    g.setColour (pink.withAlpha (0.8f));
    g.strokePath (roof, juce::PathStrokeType (1.5f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));

    // Floor: a strip of ground.
    const float ground = sky.getBottom() - 6.0f;
    g.setColour (cyan.withAlpha (0.55f));
    g.drawHorizontalLine ((int) ground, sky.getX() + 3.0f, sky.getRight() - 3.0f);

    g.setColour (edge.withAlpha (0.8f));
    g.drawRoundedRectangle (sky, 6.0f, 1.0f);

    // Source: a beam from the floor up to its height, and a level line where it is.
    const float level = juce::jlimit (ceiling, ground, pos);
    g.setGradientFill (juce::ColourGradient (pink.withAlpha (0.35f), 0.0f, level, pink.withAlpha (0.05f), 0.0f, ground, false));
    g.fillRect (juce::Rectangle<float> (sky.getCentreX() - 5.0f, level, 10.0f, ground - level));

    g.setColour (pink.withAlpha (0.7f));
    g.drawHorizontalLine ((int) level, sky.getX() + 4.0f, sky.getRight() - 4.0f);

    const auto dot = juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ sky.getCentreX(), level });
    g.setColour (pink);
    g.fillEllipse (dot);
    g.setColour (ice);
    g.fillEllipse (dot.reduced (3.5f));
}

void VaporLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    using namespace vapor;
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool primary = b.getProperties()["primary"];
    const bool live = b.getProperties()["live"];
    const float alpha = b.isEnabled() ? 1.0f : 0.28f;

    if (b.getProperties()["commit"])
    {
        // Neon outline that glows only when there's something to commit.
        if (b.isEnabled())
        {
            g.setColour (pink.withAlpha (over ? 0.22f : 0.12f));
            g.fillRoundedRectangle (r.expanded (3.0f), 9.0f);
            g.setColour (night.withAlpha (0.85f));
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (pink.withAlpha (down ? 0.3f : (over ? 0.16f : 0.08f)));
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (pink);
            g.drawRoundedRectangle (r, 6.0f, 1.6f);
            glowDot (g, { r.getX() + 15.0f, r.getCentreY() }, 7.0f, pink);
        }
        else
        {
            g.setColour (night.withAlpha (0.5f));
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (edge.withAlpha (0.6f));
            g.drawRoundedRectangle (r, 6.0f, 1.0f);
        }

        return;
    }

    if (primary)
    {
        g.setGradientFill (juce::ColourGradient (orange.withMultipliedAlpha (alpha), r.getX(), 0, pink.withMultipliedAlpha (alpha), r.getRight(), 0, false));
        g.fillRoundedRectangle (r, 6.0f);

        if (over || down)
        {
            g.setColour (juce::Colours::white.withAlpha (down ? 0.25f : 0.12f));
            g.fillRoundedRectangle (r, 6.0f);
        }

        return;
    }

    if (b.getToggleState())
    {
        g.setColour (pink.withAlpha (0.25f * alpha));
        g.fillRoundedRectangle (r.expanded (2.0f), 7.0f);
        g.setColour (pink.withMultipliedAlpha (alpha));
        g.fillRoundedRectangle (r, 5.0f);
    }
    else
    {
        g.setColour (night.withAlpha (0.7f * alpha));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour ((over ? cyan : edge).withMultipliedAlpha (alpha));
        g.drawRoundedRectangle (r, 5.0f, 1.0f);
    }

    if (live)
    {
        g.setColour (cyan.withAlpha (0.18f));
        g.fillRoundedRectangle (r.expanded (3.0f), 8.0f);
        g.setColour (cyan);
        g.drawRoundedRectangle (r, 5.0f, 2.0f);
    }
}

void VaporLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    using namespace vapor;
    const bool dark = b.getToggleState() || (bool) b.getProperties()["primary"];
    g.setFont (getTextButtonFont (b, b.getHeight()));

    if (b.getProperties()["commit"])
    {
        g.setColour (b.isEnabled() ? ice : dim.withAlpha (0.6f));
        g.drawText (b.getButtonText(), b.getLocalBounds().withTrimmedLeft (b.isEnabled() ? 18 : 0), juce::Justification::centred);
        return;
    }

    g.setColour ((dark ? night : ice).withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.35f));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

void VaporLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    using namespace vapor;
    const auto r = b.getLocalBounds().toFloat();
    const auto pill = juce::Rectangle<float> (38.0f, 20.0f).withCentre ({ r.getX() + 19.0f, r.getCentreY() });
    const bool on = b.getToggleState();

    g.setColour (on ? pink.withAlpha (0.3f) : night);
    g.fillRoundedRectangle (pill, 10.0f);
    g.setColour (on ? pink : (over ? cyan : edge));
    g.drawRoundedRectangle (pill, 10.0f, 1.5f);

    const auto knob = juce::Point<float> (on ? pill.getRight() - 10.0f : pill.getX() + 10.0f, pill.getCentreY());

    if (on)
        glowDot (g, knob, 12.0f, pink);
    else
    {
        g.setColour (dim);
        g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre (knob));
    }

    g.setFont (font (13.0f, true, 0.2f));
    g.setColour (on ? ice : dim);
    g.drawText (b.getButtonText(), r.withTrimmedLeft (48.0f), juce::Justification::centredLeft);
}

void VaporLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    using namespace vapor;
    const auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (night.withAlpha (0.75f));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour ((box.isMouseOver (true) ? cyan : edge).brighter (0.1f));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);

    juce::Path arrow;
    const float ax = (float) w - 16.0f, ay = (float) h * 0.5f;
    arrow.startNewSubPath (ax - 4.0f, ay - 2.0f);
    arrow.lineTo (ax, ay + 2.5f);
    arrow.lineTo (ax + 4.0f, ay - 2.0f);
    g.setColour (cyan);
    g.strokePath (arrow, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void VaporLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    const bool big = box.getProperties()["big"];
    label.setBounds (8, 0, box.getWidth() - 30, box.getHeight());
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (big ? juce::Justification::centred : juce::Justification::centredLeft);
}

juce::Font VaporLookAndFeel::getComboBoxFont (juce::ComboBox& box)
{
    return box.getProperties()["big"] ? vapor::font (22.0f, true, 0.06f)
         : box.getProperties()["small"] ? vapor::font (10.5f, true, 0.06f)
         : vapor::font (13.0f, true, 0.04f);
}
juce::Font VaporLookAndFeel::getTextButtonFont (juce::TextButton&, int h)      { return vapor::font (juce::jmin (13.0f, h * 0.5f), true, 0.12f); }
juce::Font VaporLookAndFeel::getLabelFont (juce::Label& l)
{
    if (auto* box = dynamic_cast<juce::ComboBox*> (l.getParentComponent()))
        return getComboBoxFont (*box);

    return l.getProperties()["value"] ? vapor::mono (12.0f) : vapor::font (11.0f, true, 0.22f);
}

juce::Label* VaporLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->getProperties().set ("value", true);
    l->setFont (vapor::mono (12.0f));
    return l;
}

//==============================================================================
/** Speaker formats Pro Tools can give a track, for the overlay when the plugin can't see one. */
static const std::vector<std::pair<juce::String, juce::AudioChannelSet>>& overlayFormats()
{
    using CS = juce::AudioChannelSet;
    static const std::vector<std::pair<juce::String, CS>> formats {
        { "Stereo", CS::stereo() },         { "5.1", CS::create5point1() },        { "7.1", CS::create7point1() },
        { "5.1.2", CS::create5point1point2() }, { "5.1.4", CS::create5point1point4() }, { "7.1.2", CS::create7point1point2() },
        { "7.1.4", CS::create7point1point4() }, { "7.1.6", CS::create7point1point6() }, { "9.1.6", CS::create9point1point6() } };
    return formats;
}

static constexpr int defaultOverlayFormat = 6; // 7.1.4

//==============================================================================
MoveButton::MoveButton (int moveIndex)
    : juce::Button (Sequencer::moveNames[moveIndex]), move (moveIndex)
{
    setTooltip (Sequencer::moveNames[moveIndex]);
}

void MoveButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    using namespace vapor;
    const auto r = getLocalBounds().toFloat().reduced (1.0f);
    const bool on = getToggleState();

    g.setColour (on ? pink.withAlpha (0.22f) : night.withAlpha (0.7f));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (on ? pink : (over ? cyan : edge));
    g.drawRoundedRectangle (r, 6.0f, on ? 1.6f : 1.0f);

    // Icon in a square in the middle, drawn in a -1..1 box.
    const auto box = r.withSizeKeepingCentre (r.getHeight() * 0.62f, r.getHeight() * 0.62f).translated (0.0f, down ? 1.0f : 0.0f);
    auto at = [&box] (float x, float y) { return juce::Point<float> (box.getCentreX() + x * box.getWidth() * 0.5f,
                                                                     box.getCentreY() - y * box.getHeight() * 0.5f); };
    juce::Path icon;
    constexpr float twoPi = juce::MathConstants<float>::twoPi;

    switch (move)
    {
        case Sequencer::point:
            break; // just the dot below

        case Sequencer::line:
            icon.startNewSubPath (at (-0.85f, -0.6f));
            icon.lineTo (at (0.85f, 0.6f));
            icon.startNewSubPath (at (0.35f, 0.62f));
            icon.lineTo (at (0.85f, 0.6f));
            icon.lineTo (at (0.62f, 0.15f));
            break;

        case Sequencer::circle:
            icon.addEllipse (box.reduced (box.getWidth() * 0.08f));
            break;

        case Sequencer::figureEight:
            for (int i = 0; i <= 48; ++i)
            {
                const float u = (float) i / 48.0f;
                const auto p = at (0.9f * std::sin (twoPi * u), 0.6f * std::sin (2.0f * twoPi * u));
                i == 0 ? icon.startNewSubPath (p) : icon.lineTo (p);
            }
            break;

        case Sequencer::zigzag:
            icon.startNewSubPath (at (-0.9f, 0.0f));
            icon.lineTo (at (-0.6f, 0.6f));
            icon.lineTo (at (-0.2f, -0.6f));
            icon.lineTo (at (0.2f, 0.6f));
            icon.lineTo (at (0.6f, -0.6f));
            icon.lineTo (at (0.9f, 0.0f));
            break;

        case Sequencer::drawn: // a pencil over a squiggle
        {
            icon.startNewSubPath (at (-0.9f, -0.55f));
            icon.cubicTo (at (-0.5f, 0.1f), at (-0.2f, -0.95f), at (0.15f, -0.45f));

            juce::Path pencil;
            pencil.startNewSubPath (at (0.15f, -0.45f));
            pencil.lineTo (at (0.3f, -0.05f));
            pencil.lineTo (at (0.9f, 0.55f));
            pencil.lineTo (at (0.65f, 0.8f));
            pencil.lineTo (at (0.05f, 0.2f));
            pencil.closeSubPath();
            icon.addPath (pencil);
            break;
        }

        default:
            break;
    }

    const auto colour = on ? ice : (over ? cyan : dim);
    g.setColour (colour);
    g.strokePath (icon, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (move == Sequencer::point || move == Sequencer::circle)
        g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (box.getCentre()));
}

//==============================================================================
int LengthBar::stepAt (float x) const
{
    const float stepWidth = (float) getWidth() / (float) Sequencer::maxSteps;
    return juce::jlimit (1, Sequencer::maxSteps, (int) std::floor (x / stepWidth) + 1);
}

void LengthBar::paint (juce::Graphics& g)
{
    using namespace vapor;
    const auto r = getLocalBounds().toFloat();
    const int steps = getSteps ? getSteps() : 1;
    const float stepWidth = r.getWidth() / (float) Sequencer::maxSteps;
    const float y = r.getCentreY();

    // Tick for every step, then the loop as a bar ending in a handle.
    for (int i = 0; i <= Sequencer::maxSteps; ++i)
    {
        g.setColour (edge.withAlpha (0.7f));
        g.drawVerticalLine (juce::roundToInt (r.getX() + (float) i * stepWidth), y - 3.0f, y + 3.0f);
    }

    g.setColour (violet);
    g.fillRoundedRectangle (juce::Rectangle<float> (r.getX(), y - 1.5f, r.getWidth(), 3.0f), 1.5f);

    const float end = r.getX() + (float) steps * stepWidth;
    g.setGradientFill (juce::ColourGradient (cyan, r.getX(), 0.0f, pink, end, 0.0f, false));
    g.fillRoundedRectangle (juce::Rectangle<float> (r.getX() + 2.0f, y - 2.0f, end - r.getX() - 4.0f, 4.0f), 2.0f);

    const auto handle = juce::Rectangle<float> (8.0f, r.getHeight() - 2.0f).withCentre ({ end - 4.0f, y });
    g.setColour (pink);
    g.fillRoundedRectangle (handle, 3.0f);
    g.setColour (ice);
    g.drawVerticalLine (juce::roundToInt (handle.getCentreX()), handle.getY() + 3.0f, handle.getBottom() - 3.0f);
}

void LengthBar::mouseDown (const juce::MouseEvent& e)
{
    if (gesture)
        gesture (true);

    mouseDrag (e);
}

void LengthBar::mouseDrag (const juce::MouseEvent& e)
{
    if (setSteps)
        setSteps (stepAt (e.position.x));

    repaint();
}

void LengthBar::mouseUp (const juce::MouseEvent&)
{
    if (gesture)
        gesture (false);
}

//==============================================================================
void PannerEditor::XYPad::setView (View v)
{
    view = v;
    updateCamera();
    repaint();
}

void PannerEditor::XYPad::updateCamera()
{
    constexpr float halfPi = juce::MathConstants<float>::halfPi;

    switch (view)
    {
        case View::top:   cam.yaw = 0.0f;  cam.pitch = halfPi;          cam.perspective = false; break;
        case View::iso:   cam.yaw = juce::MathConstants<float>::pi * 0.25f; cam.pitch = 0.6155f; cam.perspective = false; break; // true isometric
        case View::orbit: cam.yaw = orbitYaw; cam.pitch = orbitPitch;   cam.perspective = true;  break;
    }

    // Fit the room's box to the pad.
    cam.scale = 1.0f;
    cam.origin = {};
    float minX = 1.0e9f, maxX = -1.0e9f, minY = 1.0e9f, maxY = -1.0e9f;

    for (float x : { -1.0f, 1.0f })
        for (float y : { -1.0f, 1.0f })
            for (float z : { 0.0f, 1.0f })
            {
                const auto p = projectRaw (x, y, z);
                minX = juce::jmin (minX, p.x);  maxX = juce::jmax (maxX, p.x);
                minY = juce::jmin (minY, p.y);  maxY = juce::jmax (maxY, p.y);
            }

    const auto r = room();
    cam.scale = juce::jmin (r.getWidth() / juce::jmax (0.01f, maxX - minX), r.getHeight() / juce::jmax (0.01f, maxY - minY));
    cam.origin = r.getCentre() - juce::Point<float> ((minX + maxX) * 0.5f, (minY + maxY) * 0.5f) * cam.scale;
}

juce::Point<float> PannerEditor::XYPad::projectRaw (float x, float y, float z, float* depthScale) const
{
    // Turn the room by yaw about its vertical axis, then tilt by pitch. Front (y = 1) faces
    // away from the camera, so the default views look from the back of the room.
    const float height = (z - 0.5f) * roomHeight;
    const float cy = std::cos (cam.yaw), sy = std::sin (cam.yaw);
    const float cp = std::cos (cam.pitch), sp = std::sin (cam.pitch);
    const float across = x * cy - y * sy;
    const float depth = x * sy + y * cy;
    const float up = height * cp + depth * sp;
    const float away = depth * cp - height * sp;
    const float k = cam.perspective ? cameraDistance / (cameraDistance + away) : 1.0f;

    if (depthScale != nullptr)
        *depthScale = k;

    return { across * k, -up * k };
}

juce::Point<float> PannerEditor::XYPad::project (float x, float y, float z) const
{
    return cam.origin + projectRaw (x, y, z) * cam.scale;
}

float PannerEditor::XYPad::sizeAt (const Position& p) const
{
    float k = 1.0f;
    projectRaw (p.x, p.y, p.z, &k);
    return k;
}

juce::Point<float> PannerEditor::XYPad::toRoom (juce::Point<float> screen, float z) const
{
    // Inverse of projectRaw, on the horizontal plane at height z.
    const float a = (screen.x - cam.origin.x) / cam.scale;
    const float b = (cam.origin.y - screen.y) / cam.scale;
    const float height = (z - 0.5f) * roomHeight;
    const float cy = std::cos (cam.yaw), sy = std::sin (cam.yaw);
    const float cp = std::cos (cam.pitch), sp = std::sin (cam.pitch);
    float across = a, depth = 0.0f;

    if (! cam.perspective)
    {
        depth = (b - height * cp) / sp;
    }
    else
    {
        constexpr float d = cameraDistance;
        const float denominator = b * cp - d * sp;

        if (std::abs (denominator) > 1.0e-5f)
            depth = (d * height * cp - b * d + b * height * sp) / denominator;

        const float k = d / juce::jmax (0.05f, d + depth * cp - height * sp);
        across = a / k;
    }

    return { juce::jlimit (-1.0f, 1.0f, across * cy + depth * sy),
             juce::jlimit (-1.0f, 1.0f, depth * cy - across * sy) };
}

bool PannerEditor::XYPad::drawing() const
{
    return owner.selectedMove() == Sequencer::drawn;
}

float PannerEditor::XYPad::editHeight() const
{
    return owner.value (owner.targetId ('z'));
}

juce::Path PannerEditor::XYPad::movementPath (const Pattern& pattern, int step) const
{
    juce::Path path;
    constexpr int samples = 64;

    for (int i = 0; i <= samples; ++i)
    {
        const auto s = project (Sequencer::movementAt (pattern.steps, pattern.numSteps, step, (float) i / samples));

        if (i == 0)
            path.startNewSubPath (s);
        else
            path.lineTo (s);
    }

    return path;
}

void PannerEditor::XYPad::setLive (Position p, int step)
{
    live = p;
    liveStep = step;
    trail[trailHead] = p;
    trailHead = (trailHead + 1) % trail.size();
    trailFilled = trailFilled || trailHead == 0;
    repaint();
}

void PannerEditor::XYPad::paint (juce::Graphics& g)
{
    using namespace vapor;
    const auto bounds = getLocalBounds().toFloat();
    const bool top = view == View::top;

    g.setColour (night.withAlpha (0.88f));
    g.fillRoundedRectangle (bounds, 10.0f);

    auto segment = [this] (Position a, Position b)
    {
        juce::Path p;
        p.startNewSubPath (project (a));
        p.lineTo (project (b));
        return p;
    };

    auto dashed = [] (const juce::Path& path, float width, float on, float off)
    {
        const float dashes[] { on, off };
        juce::Path out;
        juce::PathStrokeType (width).createDashedStroke (out, path, dashes, 2);
        return out;
    };

    // Room: floor grid and outline, plus the ceiling and corners when seen in 3D.
    for (int i = 1; i < 8; ++i)
    {
        const float f = -1.0f + 2.0f * (float) i / 8.0f;
        g.setColour (cyan.withAlpha (i == 4 ? 0.20f : 0.07f));
        g.strokePath (segment ({ f, -1.0f, 0.0f }, { f, 1.0f, 0.0f }), juce::PathStrokeType (1.0f));
        g.strokePath (segment ({ -1.0f, f, 0.0f }, { 1.0f, f, 0.0f }), juce::PathStrokeType (1.0f));
    }

    auto outline = [this] (float z)
    {
        juce::Path p;
        p.startNewSubPath (project (-1.0f, -1.0f, z));
        p.lineTo (project (1.0f, -1.0f, z));
        p.lineTo (project (1.0f, 1.0f, z));
        p.lineTo (project (-1.0f, 1.0f, z));
        p.closeSubPath();
        return p;
    };

    glowStroke (g, outline (0.0f), cyan.withAlpha (0.55f), 1.0f, 0.6f);

    if (! top)
    {
        g.setColour (pink.withAlpha (0.35f));
        g.strokePath (outline (1.0f), juce::PathStrokeType (1.0f));

        for (float x : { -1.0f, 1.0f })
            for (float y : { -1.0f, 1.0f })
            {
                g.setColour (dim.withAlpha (0.3f));
                g.fillPath (dashed (segment ({ x, y, 0.0f }, { x, y, 1.0f }), 1.0f, 3.0f, 4.0f));
            }
    }

    g.setFont (font (9.5f, true, 0.4f));
    g.setColour (dim.withAlpha (0.7f));
    g.drawText ("FRONT", juce::Rectangle<float> (60.0f, 12.0f).withCentre (project (0.0f, 1.09f, 0.0f)), juce::Justification::centred);
    g.drawText ("REAR", juce::Rectangle<float> (60.0f, 12.0f).withCentre (project (0.0f, -1.09f, 0.0f)), juce::Justification::centred);

    const auto pattern = owner.proc.readPattern();

    // Speakers of the output format, lit by how much of the source each one gets, so Size (more
    // speakers lit) and Height (floor fades, ceiling speakers light) show up directly. From
    // above, height speakers are drawn inset as rings; in 3D they hang at the ceiling.
    {
        const auto channelSet = owner.overlayLayout();
        SpeakerLayout layout;
        layout.set (channelSet);
        float gains[SpeakerLayout::maxChannels] {};
        layout.computeGains (live, pattern.size, gains);
        g.setFont (font (9.0f, true, 0.05f));

        for (auto& s : layout.getSpeakers())
        {
            const float level = juce::jlimit (0.0f, 1.0f, gains[s.channel]);
            const Position at = top ? (s.top ? Position { s.x * 0.5f, s.y * 0.5f, 0.0f } : Position { s.x, s.y, 0.0f })
                                    : Position { s.x, s.y, s.top ? 1.0f : 0.0f };
            const auto centre = project (at);
            const float k = sizeAt (at);
            const auto colour = s.top ? pink : cyan;

            if (level > 0.02f)
            {
                const float glow = (10.0f + 22.0f * level) * k;
                juce::ColourGradient halo (colour.withAlpha (0.65f * level), centre, colour.withAlpha (0.0f),
                                           centre.translated (glow, 0.0f), true);
                g.setGradientFill (halo);
                g.fillEllipse (juce::Rectangle<float> (2.0f * glow, 2.0f * glow).withCentre (centre));
            }

            g.setColour (colour.withAlpha (0.45f + 0.55f * level));

            if (s.top)
                g.drawEllipse (juce::Rectangle<float> (12.0f * k, 12.0f * k).withCentre (centre), 1.5f + 1.5f * level);
            else
                g.fillRect (juce::Rectangle<float> ((8.0f + 3.0f * level) * k, (8.0f + 3.0f * level) * k).withCentre (centre));

            // Label: towards the middle of the room from above, just over the speaker in 3D.
            const auto name = juce::AudioChannelSet::getAbbreviatedChannelTypeName (channelSet.getTypeOfChannel (s.channel));
            const auto labelAt = top ? centre.translated (s.x * -16.0f, s.y * 15.0f + (juce::exactlyEqual (s.y, 0.0f) ? 12.0f : 0.0f))
                                     : centre.translated (0.0f, -13.0f * k);
            g.setColour (colour.withAlpha (0.55f + 0.45f * level));
            g.drawText (name, juce::Rectangle<float> (34.0f, 12.0f).withCentre (labelAt), juce::Justification::centred);
        }
    }

    const bool sequenced = owner.value ("seqOn") > 0.5f;
    const int numSteps = pattern.numSteps;
    const int editing = owner.selected;

    if (sequenced)
    {
        // Every step's movement and the glides joining them. While a step is being edited the
        // others sit faded in the background, the one before it a little brighter.
        for (int i = 0; i < numSteps; ++i)
        {
            const auto end = Sequencer::movementAt (pattern.steps, numSteps, i, 1.0f);
            const auto next = Sequencer::movementAt (pattern.steps, numSteps, (i + 1) % numSteps, 0.0f);
            g.setColour (dim.withAlpha (0.35f));
            g.fillPath (dashed (segment (end, next), 1.0f, 3.0f, 4.0f));

            if (i == editing)
                continue;

            const bool previous = editing >= 0 && i == (editing + numSteps - 1) % numSteps;
            const float alpha = editing < 0 ? 0.55f : (previous ? 0.42f : 0.2f);
            glowStroke (g, movementPath (pattern, i), (i == liveStep ? cyan : pink).withAlpha (alpha), 1.4f, 0.5f);
        }

        if (juce::isPositiveAndBelow (editing, numSteps))
        {
            glowStroke (g, movementPath (pattern, editing), pink, 2.2f);

            // Direction: an arrowhead part-way along.
            const auto pa = project (Sequencer::movementAt (pattern.steps, numSteps, editing, 0.55f));
            const auto pb = project (Sequencer::movementAt (pattern.steps, numSteps, editing, 0.58f));

            if (pa.getDistanceFrom (pb) > 0.5f)
            {
                juce::Path arrow;
                arrow.addTriangle (-6.0f, 5.0f, 6.0f, 5.0f, 0.0f, -7.0f);
                arrow.applyTransform (juce::AffineTransform::rotation (pa.getAngleToPoint (pb)).translated (pb));
                g.setColour (yellow);
                g.fillPath (arrow);
            }
        }
    }

    // Stroke in progress, on the plane it's being drawn on.
    if (stroke.size() > 1)
    {
        const float z = editHeight();
        juce::Path p;
        p.startNewSubPath (project (stroke.front().x, stroke.front().y, z));

        for (auto& pt : stroke)
            p.lineTo (project (pt.x, pt.y, z));

        glowStroke (g, p, yellow, 2.0f);
    }

    // Comet trail, oldest first.
    {
        const size_t count = trailFilled ? trail.size() : trailHead;

        for (size_t i = 1; i < count; ++i)
        {
            const auto& a = trail[(trailHead + trail.size() - count + i - 1) % trail.size()];
            const auto& b = trail[(trailHead + trail.size() - count + i) % trail.size()];
            const float age = (float) i / (float) count;
            g.setColour (cyan.interpolatedWith (pink, age).withAlpha (age * 0.6f));
            g.drawLine ({ project (a), project (b) }, (1.0f + age * 5.0f) * sizeAt (b));
        }
    }

    if (sequenced)
    {
        g.setFont (font (10.0f, true, 0.0f));

        for (int i = 0; i < numSteps; ++i)
        {
            const auto& s = pattern.steps[(size_t) i];

            if (s.move == Sequencer::drawn)
                continue; // a drawing has no centre point to show

            const auto p = project (s.position);
            const auto node = juce::Rectangle<float> (18.0f, 18.0f).withCentre (p);
            const bool isLive = i == liveStep;
            const float alpha = editing < 0 || i == editing ? 1.0f : 0.45f;

            if (! top && s.position.z > 0.01f)
            {
                g.setColour (dim.withAlpha (0.35f * alpha));
                g.fillPath (dashed (segment ({ s.position.x, s.position.y, 0.0f }, s.position), 1.0f, 2.0f, 3.0f));
            }

            g.setColour ((isLive ? pink : night).withMultipliedAlpha (alpha));
            g.fillEllipse (node);
            g.setColour ((isLive ? ice : pink).withMultipliedAlpha (alpha));
            g.drawEllipse (node, 1.5f);
            g.setColour ((isLive ? night : ice).withMultipliedAlpha (alpha));
            g.drawText (juce::String (i + 1), node, juce::Justification::centred);
        }
    }

    // Edit target
    if (! drawing())
    {
        const auto target = project (owner.value (owner.targetId ('x')), owner.value (owner.targetId ('y')), editHeight());
        g.setColour (yellow);
        g.drawEllipse (juce::Rectangle<float> (28.0f, 28.0f).withCentre (target), 1.5f);
    }
    else if (! pattern.steps[(size_t) editing].hasDrawing && stroke.empty())
    {
        g.setFont (font (13.0f, true, 0.3f));
        g.setColour (yellow);
        g.drawText ("DRAW STEP " + juce::String (editing + 1) + "'S PATH HERE", room(), juce::Justification::centred);
    }

    // Rendered position. Size spreads a ring across the floor around the source's shadow.
    const Position floorPosition { live.x, live.y, 0.0f };
    const auto floor = project (floorPosition);

    if (pattern.size > 0.01f)
    {
        juce::Path ring;

        for (int i = 0; i <= 48; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * (float) i / 48.0f;
            const auto p = project (live.x + pattern.size * std::cos (a), live.y + pattern.size * std::sin (a), 0.0f);
            i == 0 ? ring.startNewSubPath (p) : ring.lineTo (p);
        }

        ring.closeSubPath();
        g.setColour (hot.withAlpha (0.13f));
        g.fillPath (ring);
        g.setColour (hot.withAlpha (0.6f));
        g.fillPath (dashed (ring, 1.2f, 4.0f, 5.0f));
    }

    // Height: from above, the source is lifted off its shadow and grows; in 3D it is where it
    // is, with a stem down to its shadow on the floor.
    const float k = sizeAt (live);
    const auto raised = top ? floor.translated (0.0f, -live.z * 56.0f) : project (live);
    const float diameter = top ? 14.0f + 14.0f * live.z : 16.0f * k;
    const float shadow = top ? diameter : 16.0f * sizeAt (floorPosition);

    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (juce::Rectangle<float> (shadow * 1.3f, shadow * 0.5f).withCentre (floor));
    g.setColour (cyan.withAlpha (0.7f));
    g.drawEllipse (juce::Rectangle<float> (shadow * 1.3f, shadow * 0.5f).withCentre (floor), 1.0f);

    if (live.z > 0.01f)
    {
        juce::Path stem;
        stem.startNewSubPath (floor);
        stem.lineTo (raised);
        g.setColour (ice.withAlpha (0.7f));
        g.fillPath (dashed (stem, 1.5f, 3.0f, 3.0f));
    }

    glowDot (g, raised, diameter, hot);

    if (live.z > 0.01f)
    {
        g.setFont (mono (11.0f));
        g.setColour (ice);
        g.drawText ("Z " + juce::String (juce::roundToInt (live.z * 100.0f)) + "%",
                    juce::Rectangle<float> (60.0f, 14.0f).withCentre (raised.translated (diameter * 0.5f + 34.0f, 0.0f)),
                    juce::Justification::centredLeft);
    }

    if (view == View::orbit)
    {
        g.setFont (font (9.0f, true, 0.2f));
        g.setColour (dim.withAlpha (0.8f));
        g.drawText ("ALT-DRAG TO ORBIT  //  DOUBLE-CLICK TO RESET", bounds.reduced (10.0f, 4.0f), juce::Justification::bottomLeft);
    }
}

void PannerEditor::XYPad::mouseDown (const juce::MouseEvent& e)
{
    if (view == View::orbit && (e.mods.isAltDown() || e.mods.isPopupMenu() || e.mods.isCommandDown()))
    {
        orbiting = true;
        orbitStart = e.position;
        yawStart = orbitYaw;
        pitchStart = orbitPitch;
        return;
    }

    if (drawing())
    {
        stroke = { toRoom (e.position, editHeight()) };
        repaint();
        return;
    }

    for (char axis : { 'x', 'y' })
        owner.targetParam (axis)->beginChangeGesture();

    mouseDrag (e);
}

void PannerEditor::XYPad::mouseDrag (const juce::MouseEvent& e)
{
    if (orbiting)
    {
        orbitYaw = yawStart + (e.position.x - orbitStart.x) * 0.01f;
        orbitPitch = juce::jlimit (0.12f, 1.45f, pitchStart + (e.position.y - orbitStart.y) * 0.008f);
        updateCamera();
        repaint();
        return;
    }

    const auto p = toRoom (e.position, editHeight());

    if (drawing())
    {
        if (stroke.empty() || stroke.back().getDistanceFrom (p) > 0.004f)
            stroke.push_back (p);

        repaint();
        return;
    }

    auto set = [this] (char axis, float v)
    {
        auto* param = owner.targetParam (axis);
        param->setValueNotifyingHost (param->convertTo0to1 (v));
    };

    set ('x', p.x);
    set ('y', p.y);
    repaint();
}

void PannerEditor::XYPad::mouseUp (const juce::MouseEvent&)
{
    if (orbiting)
    {
        orbiting = false;
        return;
    }

    if (drawing())
    {
        if (stroke.size() > 1)
            owner.proc.setDrawing (owner.selected, stroke);

        stroke.clear();
        repaint();
        return;
    }

    for (char axis : { 'x', 'y' })
        owner.targetParam (axis)->endChangeGesture();
}

void PannerEditor::XYPad::mouseDoubleClick (const juce::MouseEvent&)
{
    if (view != View::orbit)
        return;

    orbitYaw = -0.35f;
    orbitPitch = 0.55f;
    updateCamera();
    repaint();
}

//==============================================================================
PannerEditor::PannerEditor (PannerProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (pad);

    setUpSlider (zSlider, zLabel, "HEIGHT", juce::Slider::LinearVertical);
    setUpSlider (sizeSlider, sizeLabel, "SIZE", juce::Slider::RotaryHorizontalVerticalDrag);
    setUpSlider (widthSlider, widthLabel, "WIDTH", juce::Slider::RotaryHorizontalVerticalDrag);
    setUpSlider (gainSlider, gainLabel, "GAIN", juce::Slider::RotaryHorizontalVerticalDrag);
    setUpSlider (glideSlider, glideLabel, "GLIDE", juce::Slider::RotaryHorizontalVerticalDrag);
    setUpSlider (radiusSlider, radiusLabel, "SHAPE SIZE", juce::Slider::LinearHorizontal);
    radiusSlider.setTooltip ("How big the circle, figure 8 or zigzag is.");
    glideSlider.setTooltip ("How much of each step is spent sliding to the next one. 0% jumps.");

    for (int m = 0; m < (int) moveButtons.size(); ++m)
    {
        auto& b = moveButtons[(size_t) m];
        b.onClick = [this, m] { setMove (m); };
        addAndMakeVisible (b);
    }

    lengthBar.getSteps = [this] { return juce::roundToInt (value ("seqSteps")); };
    lengthBar.setSteps = [this] (int n)
    {
        auto* param = proc.apvts.getParameter ("seqSteps");
        param->setValueNotifyingHost (param->convertTo0to1 ((float) n));
    };
    lengthBar.gesture = [this] (bool down)
    {
        auto* param = proc.apvts.getParameter ("seqSteps");
        down ? param->beginChangeGesture() : param->endChangeGesture();
    };
    lengthBar.setTooltip ("Pattern length. Drag the handle to set how many steps loop.");
    addAndMakeVisible (lengthBar);

    for (auto* b : { &rateDown, &rateUp })
        addAndMakeVisible (b);

    rateDown.onClick = [this] { stepRate (-1); };
    rateUp.onClick = [this] { stepRate (1); };

    for (auto& [name, set] : overlayFormats())
        overlayBox.addItem (name, overlayBox.getNumItems() + 1);

    overlayBox.getProperties().set ("small", true);
    overlayBox.setTooltip ("Speaker layout shown in the room. Follows the track in Bed mode.");
    overlayBox.onChange = [this]
    {
        if (! overlayFollowsTrack())
            proc.apvts.state.setProperty ("overlayFormat", overlayBox.getSelectedItemIndex(), nullptr);
    };
    addAndMakeVisible (overlayBox);

    static const char* viewNames[] { "TOP", "ISO", "3D" };

    for (int i = 0; i < (int) viewButtons.size(); ++i)
    {
        auto& b = viewButtons[(size_t) i];
        b.setButtonText (viewNames[i]);
        b.setClickingTogglesState (true);
        b.setRadioGroupId (2);
        b.onClick = [this, i]
        {
            if (viewButtons[(size_t) i].getToggleState())
            {
                pad.setView ((XYPad::View) i);
                proc.apvts.state.setProperty ("view", i, nullptr);
            }
        };
        addAndMakeVisible (b);
    }

    const int savedView = juce::jlimit (0, 2, (int) proc.apvts.state.getProperty ("view", 0));
    viewButtons[(size_t) savedView].setToggleState (true, juce::dontSendNotification);
    pad.setView ((XYPad::View) savedView);

    clearButton.onClick = [this] { if (selected >= 0) proc.setDrawing (selected, {}); };
    addAndMakeVisible (clearButton);

    moveHint.setText ("PICK A STEP BELOW TO GIVE IT A MOVE", juce::dontSendNotification);
    moveHint.setJustificationType (juce::Justification::centredLeft);
    moveHint.setColour (juce::Label::textColourId, vapor::dim);
    addAndMakeVisible (moveHint);

    auto attach = [this] (const juce::String& id, juce::Slider& s)
    {
        sliderAttachments.push_back (std::make_unique<SliderAttachment> (proc.apvts, id, s));
    };

    attach ("size", sizeSlider);
    attach ("width", widthSlider);
    attach ("gain", gainSlider);
    attach ("seqGlide", glideSlider);

    rateBox.addItemList (Sequencer::rateNames, 1);
    rateBox.getProperties().set ("big", true);
    rateBox.setTooltip ("Length of each step");
    addAndMakeVisible (rateBox);
    rateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "seqRate", rateBox);

    addAndMakeVisible (seqButton);
    seqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "seqOn", seqButton);

    manualButton.setClickingTogglesState (true);
    manualButton.setRadioGroupId (1);
    manualButton.setToggleState (true, juce::dontSendNotification);
    manualButton.onClick = [this] { if (manualButton.getToggleState()) selectTarget (-1); };
    addAndMakeVisible (manualButton);

    for (int i = 0; i < Sequencer::maxSteps; ++i)
    {
        auto& b = stepButtons[(size_t) i];
        b.setButtonText (juce::String (i + 1));
        b.setClickingTogglesState (true);
        b.setRadioGroupId (1);
        b.onClick = [this, i] { if (stepButtons[(size_t) i].getToggleState()) selectTarget (i); };
        addAndMakeVisible (b);
    }

    renderBox.addItemList (proc.apvts.getParameter ("renderMode")->getAllValueStrings(), 1);
    addAndMakeVisible (renderBox);
    renderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "renderMode", renderBox);

    bridgeStatus.getProperties().set ("value", true);
    bridgeStatus.setMinimumHorizontalScale (0.7f);
    addAndMakeVisible (bridgeStatus);

    pushButton.getProperties().set ("commit", true);
    pushButton.onClick = [this] { proc.pushToProTools(); };
    addAndMakeVisible (pushButton);

    // Added last so it sits over everything and takes every click while it's up.
    driftPrompt.onPush = [this] { proc.pushToProTools(); };
    driftPrompt.onRevert = [this] { proc.revertToLastPush(); };
    driftPrompt.onUnlink = [this] { proc.unlinkFromProTools(); };
    addChildComponent (driftPrompt);

    selectTarget (-1);
    setSize (940, 720);
    startTimerHz (60);

    proc.bridge.addChangeListener (this);
    refreshBridgeStatus();
    proc.editorOpenedOrClosed();
}

PannerEditor::~PannerEditor()
{
    proc.bridge.removeChangeListener (this);
    setLookAndFeel (nullptr);
    proc.editorOpenedOrClosed();
}

void PannerEditor::refreshBridgeStatus()
{
    using Status = ProToolsBridge::Status;
    const auto report = proc.bridge.getReport();

    juce::Colour colour = vapor::dim;
    juce::String text;

    switch (report.status)
    {
        case Status::notLinked:  text = ("NOT PUSHED. " + report.message).trimEnd(); break;
        case Status::busy:       text = report.message; colour = vapor::ice; break;
        case Status::inSync:     text = "IN SYNC. " + report.message; colour = vapor::good; break;
        case Status::drifted:    text = "DRIFT. " + report.message; colour = vapor::pink; break;
        case Status::cantVerify: text = "CAN'T VERIFY. " + report.message; colour = vapor::warning; break;
    }

    if (report.pushError.isNotEmpty())
    {
        text = report.pushError + "  |  " + text;
        colour = vapor::warning;
    }

    bridgeStatus.setText (text, juce::dontSendNotification);
    bridgeStatus.setTooltip (text);
    bridgeStatus.setColour (juce::Label::textColourId, colour);
    refreshCommitButton();

    driftPrompt.setReport (report);
    driftPrompt.setVisible (report.needsAnswer());

    if (driftPrompt.isVisible())
        driftPrompt.toFront (true);
}

bool PannerEditor::overlayFollowsTrack() const
{
    return ! proc.rendersObjects() && proc.getBusesLayout().getMainOutputChannelSet().size() > 1;
}

juce::AudioChannelSet PannerEditor::overlayLayout() const
{
    // Bed mode: the track's own output format. Object mode: Pro Tools doesn't tell the plugin
    // what the object is monitored on, so it's whatever the overlay menu says.
    if (overlayFollowsTrack())
        return proc.getBusesLayout().getMainOutputChannelSet();

    const auto& formats = overlayFormats();
    const int index = juce::jlimit (0, (int) formats.size() - 1,
                                    (int) proc.apvts.state.getProperty ("overlayFormat", defaultOverlayFormat));
    return formats[(size_t) index].second;
}

void PannerEditor::refreshCommitButton()
{
    const bool busy = proc.bridge.getReport().status == ProToolsBridge::Status::busy;
    const bool objects = proc.rendersObjects();
    const bool changes = objects && ! busy && proc.differsFromWrittenAutomation();

    const juce::String text = busy ? "WRITING..."
                            : ! objects ? "NO AUTOMATION"
                            : changes ? "COMMIT AUTOMATION"
                            : "AUTOMATION IN SYNC";

    const juce::String tip = ! objects ? "Bed mode: Panner pans the audio itself, so there's nothing to write."
                           : changes ? "Write the steps into the track's pan automation over the Pro Tools timeline selection."
                           : "Pro Tools' pan automation already matches the steps.";

    if (pushButton.isEnabled() != changes)
        pushButton.setEnabled (changes);

    if (pushButton.getButtonText() != text)
        pushButton.setButtonText (text);

    pushButton.setTooltip (tip);
}

void PannerEditor::setUpSlider (juce::Slider& s, juce::Label& l, const juce::String& text, juce::Slider::SliderStyle style)
{
    const bool horizontal = style == juce::Slider::LinearHorizontal;
    s.setSliderStyle (style);
    s.setTextBoxStyle (horizontal ? juce::Slider::TextBoxRight : juce::Slider::TextBoxBelow, false, 62, 18);
    addAndMakeVisible (s);

    l.setText (text, juce::dontSendNotification);
    l.setJustificationType (horizontal ? juce::Justification::centredLeft : juce::Justification::centred);
    l.attachToComponent (&s, horizontal);
}

juce::String PannerEditor::targetId (char axis) const
{
    return selected < 0 ? juce::String::charToString ((juce::juce_wchar) axis)
                        : PannerProcessor::stepParamId (selected, axis);
}

void PannerEditor::stepRate (int delta)
{
    // Rates in order from slowest to fastest, whatever their order in the menu.
    std::vector<int> order ((size_t) Sequencer::rateNames.size());
    std::iota (order.begin(), order.end(), 0);
    std::stable_sort (order.begin(), order.end(), [] (int a, int b) { return Sequencer::rateInBeats (a) > Sequencer::rateInBeats (b); });

    const int current = juce::roundToInt (value ("seqRate"));
    const auto at = (int) (std::find (order.begin(), order.end(), current) - order.begin());
    const int next = order[(size_t) juce::jlimit (0, (int) order.size() - 1, at + delta)];

    auto* p = proc.apvts.getParameter ("seqRate");
    p->beginChangeGesture();
    p->setValueNotifyingHost (p->convertTo0to1 ((float) next));
    p->endChangeGesture();
}

void PannerEditor::selectTarget (int step)
{
    selected = step;
    zAttachment.reset();
    zAttachment = std::make_unique<SliderAttachment> (proc.apvts, targetId ('z'), zSlider);

    radiusAttachment.reset();

    if (step >= 0)
        radiusAttachment = std::make_unique<SliderAttachment> (proc.apvts, PannerProcessor::stepParamId (step, 'r'), radiusSlider);

    timerCallback();
    repaint();
}

void PannerEditor::setMove (int move)
{
    if (selected < 0)
        return;

    if (auto* p = proc.apvts.getParameter (PannerProcessor::stepParamId (selected, 'm')))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) move));
        p->endChangeGesture();
    }

    // Moves only play while the sequencer runs; turn it on so the choice is heard.
    if (auto* seq = proc.apvts.getParameter ("seqOn"); seq->getValue() < 0.5f)
        seq->setValueNotifyingHost (1.0f);
}

void PannerEditor::timerCallback()
{
    const int numSteps = juce::roundToInt (value ("seqSteps"));
    int step = -1;
    const auto position = proc.displayPosition (step);

    for (int i = 0; i < Sequencer::maxSteps; ++i)
    {
        auto& b = stepButtons[(size_t) i];
        b.setEnabled (i < numSteps);

        if ((bool) b.getProperties()["live"] != (i == step))
        {
            b.getProperties().set ("live", i == step);
            b.repaint();
        }
    }

    if (selected >= numSteps)
        manualButton.setToggleState (true, juce::sendNotification);

    lengthBar.repaint();

    // A few times a second is plenty for the commit button and the overlay menu.
    if (++commitCheckFrames >= 6)
    {
        commitCheckFrames = 0;
        refreshCommitButton();

        const bool follows = overlayFollowsTrack();
        overlayBox.setEnabled (! follows);
        const auto set = overlayLayout();
        const auto& formats = overlayFormats();
        const auto match = std::find_if (formats.begin(), formats.end(), [&set] (const auto& f) { return f.second == set; });

        if (match != formats.end())
        {
            const int index = (int) (match - formats.begin());

            if (overlayBox.getSelectedItemIndex() != index)
            {
                overlayBox.setSelectedItemIndex (index, juce::dontSendNotification);
                pad.repaint();
            }
        }
    }

    // Move controls follow the selected step, including changes from automation.
    const int move = selectedMove();
    const bool stepSelected = selected >= 0;

    for (int m = 0; m < (int) moveButtons.size(); ++m)
    {
        auto& b = moveButtons[(size_t) m];
        b.setVisible (stepSelected);

        if (b.getToggleState() != (m == move))
            b.setToggleState (m == move, juce::dontSendNotification);
    }

    const bool shaped = move == Sequencer::circle || move == Sequencer::figureEight || move == Sequencer::zigzag;
    radiusSlider.setVisible (stepSelected && shaped);
    clearButton.setVisible (stepSelected && move == Sequencer::drawn);
    moveHint.setVisible (! stepSelected || ! (shaped || move == Sequencer::drawn));
    moveHint.setBounds (stepSelected ? moveHintLow : moveHintHigh);
    moveHint.setText (! stepSelected ? "PICK A STEP BELOW TO GIVE IT A MOVE"
                                     : (move == Sequencer::line ? "LINE: TRAVELS STRAIGHT TO THE NEXT STEP"
                                                                : "POINT: HOLDS STILL ON ITS SPOT"),
                      juce::dontSendNotification);

    const auto title = stepSelected ? "STEP " + juce::String (selected + 1) + " MOVE  //  "
                                          + Sequencer::moveNames[juce::jlimit (0, Sequencer::moveNames.size() - 1, move)].toUpperCase()
                                    : juce::String ("MOVE");

    if (moveTitleText != title)
    {
        moveTitleText = title;
        repaint (moveTitle);
    }

    pad.setLive (position, step);
}

/** The stencil mark from the plugin icon (clef, glasses, mohawk), coloured like a sunset sun:
    yellow to pink, with widening dark bands across its lower part. heightPx is the image
    height; the width follows the mark's shape. */
static juce::Image makeSunLogo (int heightPx)
{
    using namespace vapor;
    const auto icon = juce::ImageCache::getFromMemory (BinaryData::pluginicon_png, BinaryData::pluginicon_pngSize);

    if (! icon.isValid())
        return {};

    const juce::Rectangle<int> mark (330, 170, 370, 520); // the mark's area in the 1024px icon
    const int h = heightPx;
    const int wPx = juce::roundToInt ((float) h * (float) mark.getWidth() / (float) mark.getHeight());
    const auto src = icon.getClippedImage (mark).rescaled (wPx, h, juce::Graphics::highResamplingQuality);

    juce::Image out (juce::Image::ARGB, wPx, h, true);
    const juce::Image::BitmapData in (src, juce::Image::BitmapData::readOnly);
    juce::Image::BitmapData dest (out, juce::Image::BitmapData::writeOnly);

    juce::ColourGradient sun (yellow, 0.0f, 0.0f, pink, 0.0f, (float) h, false);
    sun.addColour (0.5, orange);

    const float bandsFrom = 0.42f, period = 0.055f;

    for (int y = 0; y < h; ++y)
    {
        const float t = (float) y / (float) h;
        bool cut = false;

        if (t > bandsFrom)
        {
            const float depth = (t - bandsFrom) / (1.0f - bandsFrom); // 0..1 down the bands
            cut = std::fmod (t - bandsFrom, period) < period * (0.18f + 0.5f * depth);
        }

        if (cut)
            continue;

        const auto colour = sun.getColourAtPosition (t);

        for (int x = 0; x < wPx; ++x)
        {
            const auto p = in.getPixelColour (x, y);
            const float a = juce::jlimit (0.0f, 1.0f, (p.getPerceivedBrightness() - 0.3f) / 0.5f) * p.getFloatAlpha();

            if (a > 0.0f)
                dest.setPixelColour (x, y, colour.withAlpha (a));
        }
    }

    return out;
}

void PannerEditor::paint (juce::Graphics& g)
{
    using namespace vapor;
    const auto bounds = getLocalBounds().toFloat();
    const float w = bounds.getWidth();
    const float horizon = (float) header.getBottom();

    // Dusk sky
    juce::ColourGradient sky (night, 0, 0, dusk, 0, bounds.getHeight(), false);
    sky.addColour (0.45, violet.darker (0.3f));
    g.setGradientFill (sky);
    g.fillAll();

    // The Anti-Music stencil as the sunset sun, rising out of the horizon.
    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (header.withX (0).withWidth ((int) w));
        const float cx = w * 0.43f;

        juce::ColourGradient halo (pink.withAlpha (0.38f), cx, horizon, pink.withAlpha (0.0f), cx, horizon - 110.0f, true);
        halo.addColour (0.35, orange.withAlpha (0.16f));
        g.setGradientFill (halo);
        g.fillEllipse (juce::Rectangle<float> (240.0f, 220.0f).withCentre ({ cx, horizon }));

        if (! sunLogo.isValid())
            sunLogo = makeSunLogo (sunLogoHeight * 3);

        if (sunLogo.isValid())
        {
            const float lh = (float) sunLogoHeight;
            const float lw = lh * (float) sunLogo.getWidth() / (float) sunLogo.getHeight();
            g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
            g.drawImage (sunLogo, { cx - lw * 0.5f, horizon - lh * sunLogoRise, lw, lh });
        }
    }

    // Neon perspective floor from the horizon down.
    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (getLocalBounds().withTop ((int) horizon));
        const float vanishX = w * 0.5f;

        for (int i = -14; i <= 14; ++i)
        {
            g.setColour (pink.withAlpha (0.16f));
            g.drawLine (vanishX + (float) i * 20.0f, horizon, vanishX + (float) i * 150.0f, bounds.getBottom(), 1.0f);
        }

        for (int i = 1; i < 16; ++i)
        {
            const float t = (float) i / 16.0f;
            const float y = horizon + (bounds.getBottom() - horizon) * t * t;
            g.setColour (pink.withAlpha (0.05f + 0.15f * t));
            g.drawHorizontalLine ((int) y, 0.0f, w);
        }
    }

    // Horizon line
    {
        juce::Path line;
        line.startNewSubPath (0.0f, horizon);
        line.lineTo (w, horizon);
        glowStroke (g, line, pink, 1.5f, 0.8f);
    }

    // Title, with a cyan ghost behind it.
    auto titleArea = header.reduced (20, 10).toFloat();
    g.setFont (font (12.0f, true, 0.45f));
    g.setColour (cyan);
    g.drawText ("ANTI-MUSIC", titleArea.removeFromTop (16.0f), juce::Justification::bottomLeft);

    const auto bigTitle = titleArea.withTrimmedTop (2.0f);
    g.setFont (font (32.0f, true, 0.08f).italicised());
    g.setColour (cyan.withAlpha (0.55f));
    g.drawText ("SPATIAL PANNER", bigTitle.translated (2.5f, 2.0f), juce::Justification::topLeft);
    g.setGradientFill (juce::ColourGradient (yellow, 0, bigTitle.getY(), pink, 0, bigTitle.getY() + 34.0f, false));
    g.drawText ("SPATIAL PANNER", bigTitle, juce::Justification::topLeft);

    neonPanel (g, controlsPanel.toFloat());
    neonPanel (g, sequencerPanel.toFloat());

    auto source = controlsPanel.reduced (16, 10);
    sectionTitle (g, "SOURCE", source.removeFromTop (18));
    sectionTitle (g, "PATTERN", patternTitle);
    sectionTitle (g, "RATE  //  EACH STEP LASTS", rateTitle);
    sectionTitle (g, moveTitleText, moveTitle);
    sectionTitle (g, "STEPS  //  CLICK ONE, THEN DRAG IN THE ROOM.  DRAG THE BAR TO SET LENGTH",
                  sequencerPanel.reduced (14, 6).withHeight (14));

    // Status bar
    g.setColour (night.withAlpha (0.8f));
    g.fillRect (statusBar);
    g.setColour (edge);
    g.drawHorizontalLine (statusBar.getY(), 0.0f, w);

    // Scanlines
    g.setColour (juce::Colours::black.withAlpha (0.10f));

    for (int y = 0; y < getHeight(); y += 3)
        g.drawHorizontalLine (y, 0.0f, w);
}

void PannerEditor::resized()
{
    driftPrompt.setBounds (getLocalBounds());

    auto r = getLocalBounds();
    header = r.removeFromTop (84);
    statusBar = r.removeFromBottom (28);
    bridgeStatus.setBounds (statusBar.reduced (14, 0));

    // Header controls
    auto headerControls = header.reduced (20, 24);
    pushButton.setBounds (headerControls.removeFromRight (196));
    headerControls.removeFromRight (10);
    renderBox.setBounds (headerControls.removeFromRight (210));

    r.reduce (16, 14);
    sequencerPanel = r.removeFromBottom (92);
    r.removeFromBottom (12);

    // Steps row, with the length bar underneath lined up with the step buttons.
    {
        auto row = sequencerPanel.reduced (14, 8);
        row.removeFromTop (20);
        auto bar = row.removeFromBottom (14);
        row.removeFromBottom (4);
        manualButton.setBounds (row.removeFromLeft (92));
        row.removeFromLeft (10);
        bar.removeFromLeft (102);
        const int stepWidth = row.getWidth() / Sequencer::maxSteps;
        lengthBar.setBounds (bar.withWidth (stepWidth * Sequencer::maxSteps));

        for (auto& b : stepButtons)
            b.setBounds (row.removeFromLeft (stepWidth).reduced (2, 0));
    }

    pad.setBounds (r.removeFromLeft (r.getHeight()));
    overlayBox.setBounds (pad.getRight() - 84, pad.getY() + 3, 80, 18);

    for (int i = 0; i < (int) viewButtons.size(); ++i)
        viewButtons[(size_t) i].setBounds (pad.getX() + 4 + i * 38, pad.getY() + 3, 36, 18);
    r.removeFromLeft (8);

    auto zArea = r.removeFromLeft (58);
    zArea.removeFromTop (24);
    zSlider.setBounds (zArea);
    r.removeFromLeft (10);

    controlsPanel = r;
    auto c = r.reduced (16, 10);
    c.removeFromTop (24);

    auto knobs = c.removeFromTop (118);
    knobs.removeFromTop (16);
    const int knobWidth = knobs.getWidth() / 3;
    sizeSlider.setBounds (knobs.removeFromLeft (knobWidth));
    widthSlider.setBounds (knobs.removeFromLeft (knobWidth));
    gainSlider.setBounds (knobs);

    c.removeFromTop (10);
    patternTitle = c.removeFromTop (18);
    c.removeFromTop (6);
    seqButton.setBounds (c.removeFromTop (26));
    c.removeFromTop (10);

    // Rate gets the room: a big readout with arrows. Glide is a knob beside it.
    {
        auto row = c.removeFromTop (104);
        auto glideArea = row.removeFromRight (104);
        glideSlider.setBounds (glideArea.withTrimmedTop (16));
        row.removeFromRight (12);

        rateTitle = row.removeFromTop (16);
        row.removeFromTop (14);
        auto stepper = row.removeFromTop (52);
        rateDown.setBounds (stepper.removeFromLeft (38));
        rateUp.setBounds (stepper.removeFromRight (38));
        rateBox.setBounds (stepper.reduced (6, 0));
    }

    c.removeFromTop (12);
    moveTitle = c.removeFromTop (18);
    c.removeFromTop (8);

    {
        auto line = c.removeFromTop (44);
        const int w = line.getWidth() / (int) moveButtons.size();

        for (auto& b : moveButtons)
            b.setBounds (line.removeFromLeft (w).reduced (3, 0));
    }

    constexpr int labelWidth = 84;
    c.removeFromTop (12);
    auto last = c.removeFromTop (28);
    radiusSlider.setBounds (last.withTrimmedLeft (labelWidth));
    clearButton.setBounds (last.reduced (3, 0));
    moveHintLow = last;
    moveHintHigh = moveTitle.translated (0, 26).withHeight (28);
}
