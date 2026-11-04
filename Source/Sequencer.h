#pragma once

#include <juce_graphics/juce_graphics.h>
#include "SpatialPanner.h"

/** Tempo-synced step sequencer for source position. Stateless: the position is a pure
    function of the host's musical position, so offline bounces match realtime playback.

    Each step plays one movement over its length: a preset shape around the step's point, or
    a path drawn by hand. Glide then carries the source from where the movement ends to where
    the next step's movement begins. */
struct Sequencer
{
    static constexpr int maxSteps = 16;
    static constexpr int drawingPoints = 64;

    enum Move { point, line, circle, figureEight, zigzag, drawn };

    static inline const juce::StringArray moveNames { "Point", "Line", "Circle", "Figure 8", "Zigzag", "Draw" };

    /** A hand-drawn path in room coordinates, evenly spaced along its length. */
    using Drawing = std::array<juce::Point<float>, drawingPoints>;

    struct Step
    {
        Position position;     // the point, or the centre of a shape
        int move = point;
        float radius = 0.3f;
        bool hasDrawing = false;
        Drawing drawing {};
    };

    using Steps = std::array<Step, maxSteps>;

    static inline const juce::StringArray rateNames { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32",
                                                      "1/4T", "1/8T", "1/16T", "1/4.", "1/8." };

    /** Step length in quarter notes for an index into rateNames. */
    static double rateInBeats (int rateIndex)
    {
        static constexpr double beats[] { 4.0, 2.0, 1.0, 0.5, 0.25, 0.125,
                                          2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0, 1.5, 0.75 };
        return beats[juce::jlimit (0, (int) std::size (beats) - 1, rateIndex)];
    }

    static int stepAt (double ppq, double stepBeats, int numSteps)
    {
        const auto index = (juce::int64) std::floor (ppq / stepBeats);
        return (int) (((index % numSteps) + numSteps) % numSteps); // ppq is negative during pre-roll
    }

    /** True when a step's movement is a curve, so automation needs many breakpoints for it. */
    static bool isCurved (const Step& s)
    {
        return s.move == circle || s.move == figureEight || s.move == zigzag || (s.move == drawn && s.hasDrawing);
    }

    /** Where step `index`'s movement puts the source, u running 0..1 across the movement. */
    static Position movementAt (const Steps& steps, int numSteps, int index, float u)
    {
        const auto& s = steps[(size_t) index];
        const auto& p = s.position;
        constexpr float twoPi = juce::MathConstants<float>::twoPi;

        auto clamped = [] (float x, float y, float z)
        {
            return Position { juce::jlimit (-1.0f, 1.0f, x), juce::jlimit (-1.0f, 1.0f, y), z };
        };

        auto towardsNext = [&] (float amount)
        {
            const auto next = movementAt (steps, numSteps, (index + 1) % numSteps, 0.0f);
            return Position { p.x + (next.x - p.x) * amount, p.y + (next.y - p.y) * amount, p.z + (next.z - p.z) * amount };
        };

        switch (s.move)
        {
            case line:
                return u <= 0.0f ? p : towardsNext (u);

            case circle: // starts and ends at the top of the circle
                return clamped (p.x + s.radius * std::sin (twoPi * u), p.y + s.radius * std::cos (twoPi * u), p.z);

            case figureEight: // starts and ends at the centre
                return clamped (p.x + s.radius * std::sin (twoPi * u), p.y + 0.5f * s.radius * std::sin (2.0f * twoPi * u), p.z);

            case zigzag: // three zigs across the straight line to the next step
            {
                if (u <= 0.0f)
                    return p;

                const auto base = towardsNext (u);
                const auto next = movementAt (steps, numSteps, (index + 1) % numSteps, 0.0f);
                auto dx = next.x - p.x, dy = next.y - p.y;
                const float length = std::sqrt (dx * dx + dy * dy);

                if (length < 1.0e-4f)
                    dx = 1.0f, dy = 0.0f;
                else
                    dx /= length, dy /= length;

                const float phase = std::fmod (u * 3.0f, 1.0f);
                const float tri = phase < 0.25f ? phase * 4.0f : (phase < 0.75f ? 2.0f - phase * 4.0f : phase * 4.0f - 4.0f);
                return clamped (base.x - dy * s.radius * tri, base.y + dx * s.radius * tri, base.z);
            }

            case drawn:
            {
                if (! s.hasDrawing)
                    return p;

                const float f = juce::jlimit (0.0f, 1.0f, u) * (float) (drawingPoints - 1);
                const int i = juce::jmin ((int) f, drawingPoints - 2);
                const auto pt = s.drawing[(size_t) i] + (s.drawing[(size_t) i + 1] - s.drawing[(size_t) i]) * (f - (float) i);
                return clamped (pt.x, pt.y, p.z);
            }

            default:
                return p;
        }
    }

    /** glide is the fraction (0..1) of each step spent travelling to the next one. */
    static Position positionAt (double ppq, double stepBeats, int numSteps, float glide, const Steps& steps)
    {
        const double t = ppq / stepBeats;
        const auto phase = (float) (t - std::floor (t));
        const int current = stepAt (ppq, stepBeats, numSteps);
        const float moveEnd = 1.0f - glide;

        if (glide <= 0.0f || phase <= moveEnd)
            return movementAt (steps, numSteps, current, moveEnd > 0.0f ? phase / moveEnd : 1.0f);

        const auto a = movementAt (steps, numSteps, current, 1.0f);
        const auto b = movementAt (steps, numSteps, (current + 1) % numSteps, 0.0f);
        const float mix = (phase - moveEnd) / glide;

        return { a.x + (b.x - a.x) * mix,
                 a.y + (b.y - a.y) * mix,
                 a.z + (b.z - a.z) * mix };
    }

    /** Resamples a freehand stroke to evenly spaced points along its length. */
    static Drawing resample (const std::vector<juce::Point<float>>& stroke)
    {
        Drawing out {};

        if (stroke.empty())
            return out;

        std::vector<float> distance { 0.0f };

        for (size_t i = 1; i < stroke.size(); ++i)
            distance.push_back (distance.back() + stroke[i].getDistanceFrom (stroke[i - 1]));

        const float total = distance.back();
        size_t seg = 0;

        for (int k = 0; k < drawingPoints; ++k)
        {
            const float target = total * (float) k / (float) (drawingPoints - 1);

            while (seg + 2 < stroke.size() && distance[seg + 1] < target)
                ++seg;

            if (stroke.size() == 1 || total <= 0.0f)
            {
                out[(size_t) k] = stroke.front();
                continue;
            }

            const float span = distance[seg + 1] - distance[seg];
            const float f = span > 0.0f ? juce::jlimit (0.0f, 1.0f, (target - distance[seg]) / span) : 0.0f;
            out[(size_t) k] = stroke[seg] + (stroke[seg + 1] - stroke[seg]) * f;
        }

        return out;
    }
};
