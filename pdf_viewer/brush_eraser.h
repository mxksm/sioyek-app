#pragma once
#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>

namespace brush_eraser {
struct Point { double x, y; };
using Interval = std::pair<double, double>;

inline double point_distance_squared(Point p, Point a, Point b) {
    const double dx = b.x - a.x, dy = b.y - a.y;
    const double length = dx * dx + dy * dy;
    const double t = length > 0
        ? std::clamp(((p.x - a.x) * dx + (p.y - a.y) * dy) / length, 0.0, 1.0) : 0;
    const double x = p.x - a.x - t * dx, y = p.y - a.y - t * dy;
    return x * x + y * y;
}

// Clip a linear coordinate to a slab while retaining its segment parameters.
inline bool clip_axis(double origin, double delta, double min, double max, Interval& interval) {
    if (std::abs(delta) < 1e-12) return origin >= min && origin <= max;
    double a = (min - origin) / delta, b = (max - origin) / delta;
    if (a > b) std::swap(a, b);
    interval.first = std::max(interval.first, a);
    interval.second = std::min(interval.second, b);
    return interval.first <= interval.second;
}

// Return the part of stroke segment a--b inside the swept circular brush.
// Its footprint is a capsule: an oriented rectangle plus two circular caps.
// The capsule is convex, so its intersection with a segment is one interval.
inline std::optional<Interval> erased_interval(Point a, Point b, Point from, Point to, double radius) {
    // Most segments are nowhere near this mouse move. Reject them before the
    // capsule's square roots, projections and circle intersections.
    if (std::max(a.x,b.x) < std::min(from.x,to.x)-radius ||
        std::min(a.x,b.x) > std::max(from.x,to.x)+radius ||
        std::max(a.y,b.y) < std::min(from.y,to.y)-radius ||
        std::min(a.y,b.y) > std::max(from.y,to.y)+radius) return {};
    const double dx = b.x - a.x, dy = b.y - a.y;
    const double length_squared = dx * dx + dy * dy;
    if (length_squared < 1e-20) {
        if (point_distance_squared(a, from, to) <= radius * radius) return Interval{0, 1};
        return {};
    }
    std::optional<Interval> result;
    auto include = [&](Interval interval) {
        interval.first = std::max(0.0, interval.first);
        interval.second = std::min(1.0, interval.second);
        if (interval.first > interval.second) return;
        if (!result) result = interval;
        else {
            result->first = std::min(result->first, interval.first);
            result->second = std::max(result->second, interval.second);
        }
    };
    auto circle = [&](Point center) {
        const double x = a.x - center.x, y = a.y - center.y;
        const double projection = x * dx + y * dy;
        // Use perpendicular distance, avoiding cancellation for long segments.
        const double cross = x * dy - y * dx;
        const double remaining = radius * radius - cross * cross / length_squared;
        if (remaining < 0) return;
        const double middle = -projection / length_squared;
        const double half = std::sqrt(remaining / length_squared);
        include({middle - half, middle + half});
    };
    circle(from);
    circle(to);
    const double bx = to.x - from.x, by = to.y - from.y;
    const double brush_length = std::hypot(bx, by);
    if (brush_length > 1e-10) {
        const double ux = bx / brush_length, uy = by / brush_length;
        const double x = a.x - from.x, y = a.y - from.y;
        Interval rectangle{0, 1};
        if (clip_axis(x * ux + y * uy, dx * ux + dy * uy, 0, brush_length, rectangle) &&
            clip_axis(-x * uy + y * ux, -dx * uy + dy * ux, -radius, radius, rectangle)) {
            include(rectangle);
        }
    }
    return result;
}
}
