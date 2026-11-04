#pragma once

#include "Sequencer.h"

/** Everything that decides where the source is at a given musical time. */
struct Pattern
{
    Position manual;
    float size = 0.0f;
    float width = 0.0f;   // stereo offset each side, applied to x
    bool sequenced = false;
    int numSteps = 1;
    double stepBeats = 1.0;
    float glide = 0.0f;
    Sequencer::Steps steps {};

    Position at (double beat) const
    {
        return sequenced ? Sequencer::positionAt (beat, stepBeats, numSteps, glide, steps) : manual;
    }
};

/** True when two patterns would write the same automation. Ignores settings that don't reach
    the lanes, such as steps past the step count or the radius of a move without one. */
inline bool samePattern (const Pattern& a, const Pattern& b)
{
    auto near = [] (float u, float v) { return std::abs (u - v) < 1.0e-4f; };
    auto samePosition = [&] (const Position& p, const Position& q) { return near (p.x, q.x) && near (p.y, q.y) && near (p.z, q.z); };

    if (! near (a.size, b.size) || ! near (a.width, b.width) || a.sequenced != b.sequenced)
        return false;

    if (! a.sequenced)
        return samePosition (a.manual, b.manual);

    if (a.numSteps != b.numSteps || ! juce::exactlyEqual (a.stepBeats, b.stepBeats) || ! near (a.glide, b.glide))
        return false;

    for (size_t i = 0; i < (size_t) a.numSteps; ++i)
    {
        const auto& s = a.steps[i];
        const auto& t = b.steps[i];

        if (! samePosition (s.position, t.position) || s.move != t.move)
            return false;

        const bool hasRadius = s.move == Sequencer::circle || s.move == Sequencer::figureEight || s.move == Sequencer::zigzag;

        if (hasRadius && ! near (s.radius, t.radius))
            return false;

        if (s.move == Sequencer::drawn)
        {
            if (s.hasDrawing != t.hasDrawing)
                return false;

            if (s.hasDrawing)
                for (size_t k = 0; k < s.drawing.size(); ++k)
                    if (! near (s.drawing[k].x, t.drawing[k].x) || ! near (s.drawing[k].y, t.drawing[k].y))
                        return false;
        }
    }

    return true;
}

/** Turns a pattern into automation breakpoints, and checks breakpoints against a pattern.

    Between the times returned by keyTimes() the pattern moves in straight lines, or, inside
    curved and drawn moves, close enough to straight that a host joining breakpoints with
    straight lines reproduces it. */
namespace Bake
{
    constexpr double ticksPerBeat = 960000.0; // Pro Tools ticks per quarter note
    constexpr double oneTick = 1.0 / ticksPerBeat;
    constexpr int curveSamplesPerStep = 48; // per beat of movement, capped per step

    struct Point
    {
        double beat;
        float value;
    };

    using Curve = std::vector<Point>;

    inline std::vector<double> keyTimes (const Pattern& p, double start, double end)
    {
        std::vector<double> times { start, end };

        if (p.sequenced)
        {
            for (double k = std::floor (start / p.stepBeats); k * p.stepBeats <= end; k += 1.0)
            {
                const double boundary = k * p.stepBeats;

                const double moveBeats = (1.0 - p.glide) * p.stepBeats;

                if (p.glide > 0.0f)
                {
                    times.push_back (boundary);                                    // arrive at step
                    times.push_back (boundary + moveBeats);                        // start gliding on
                }
                else
                {
                    times.push_back (boundary - oneTick);                          // last tick of previous step
                    times.push_back (boundary);                                    // jump
                }

                // Curves and drawings: enough points that straight lines between them follow
                // the shape. Line and Point moves are already straight.
                const auto& step = p.steps[(size_t) Sequencer::stepAt (boundary, p.stepBeats, p.numSteps)];

                if (Sequencer::isCurved (step) && moveBeats > 0.0)
                {
                    const int samples = juce::jlimit (8, curveSamplesPerStep, (int) std::ceil (moveBeats * curveSamplesPerStep));

                    for (int i = 1; i < samples; ++i)
                        times.push_back (boundary + moveBeats * i / samples);
                }
            }
        }

        times.erase (std::remove_if (times.begin(), times.end(), [=] (double t) { return t < start || t > end; }),
                     times.end());
        std::sort (times.begin(), times.end());
        times.erase (std::unique (times.begin(), times.end(), [] (double a, double b) { return b - a < oneTick * 0.5; }),
                     times.end());
        return times;
    }

    inline Curve curve (const Pattern& p, double start, double end, const std::function<float (Position)>& value)
    {
        Curve c;

        for (auto t : keyTimes (p, start, end))
            c.push_back ({ t, value (p.at (t)) });

        return c;
    }

    /** Value of a curve joined with straight lines, held flat beyond its ends. */
    inline float valueAt (const Curve& c, double beat)
    {
        if (c.empty())
            return 0.0f;

        if (beat <= c.front().beat)
            return c.front().value;

        if (beat >= c.back().beat)
            return c.back().value;

        auto next = std::upper_bound (c.begin(), c.end(), beat, [] (double b, const Point& p) { return b < p.beat; });
        auto prev = std::prev (next);
        const double span = next->beat - prev->beat;

        if (span <= 0.0)
            return next->value;

        return prev->value + (next->value - prev->value) * (float) ((beat - prev->beat) / span);
    }

    /** True when the two curves agree everywhere in [start, end] to within tolerance. Allows a
        couple of ticks of timing slop so a host rounding breakpoint times isn't called drift. */
    inline bool matches (const Curve& expected, const Curve& actual, double start, double end, float tolerance)
    {
        if (actual.empty())
            return false;

        constexpr double slop = 2.0 * oneTick;

        auto agreesAt = [&] (const Curve& a, const Curve& b, double t)
        {
            const float v = valueAt (a, t);

            for (double offset : { 0.0, -slop, slop })
                if (std::abs (valueAt (b, t + offset) - v) <= tolerance)
                    return true;

            return false;
        };

        for (auto& p : expected)
            if (p.beat >= start && p.beat <= end && ! agreesAt (expected, actual, p.beat))
                return false;

        for (auto& p : actual)
            if (p.beat >= start && p.beat <= end && ! agreesAt (actual, expected, p.beat))
                return false;

        return true;
    }
}
