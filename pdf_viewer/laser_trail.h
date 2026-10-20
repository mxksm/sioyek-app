#pragma once
#include <algorithm>

namespace laser_trail {
// No part of a held stroke ages. After release the fade sweeps from head to tail.
inline double opacity(double fraction, double elapsed_since_release_ms) {
    if (elapsed_since_release_ms < 0) return 1;
    const double t = std::clamp((elapsed_since_release_ms - 250.0 -
        900.0 * std::clamp(fraction, 0.0, 1.0)) / 300.0, 0.0, 1.0);
    return 1.0 - t*t*(3.0-2.0*t);
}
constexpr double lifetime_ms = 1450;
}
