#pragma once
#include <array>
#include <cmath>

struct SoftShadowTap { float x, y, weight; };
// Symmetric, fixed screen-space kernel: stable during animation and camera cuts.
inline constexpr std::array<SoftShadowTap, 13> kSoftShadowTaps = {{
    {0, 0, .20f},
    {1, 0, .10f}, {-1, 0, .10f}, {0, 1, .10f}, {0, -1, .10f},
    {.9f, .9f, .05f}, {-.9f, .9f, .05f}, {.9f, -.9f, .05f}, {-.9f, -.9f, .05f},
    {2, 0, .05f}, {-2, 0, .05f}, {0, 2, .05f}, {0, -2, .05f}
}};

inline float softShadowTapAlpha(float weight)
{
    // Normalise source-over compositing: full coverage stays at 38% opacity,
    // independent of tap count, rather than darkening once per body polygon.
    return 1.f - std::pow(1.f - .38f, weight);
}
