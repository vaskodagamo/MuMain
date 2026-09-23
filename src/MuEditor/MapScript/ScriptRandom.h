#pragma once

#ifdef _EDITOR

#include <cstdint>

namespace Editor::MapScript
{
// The random numbers of an edit script (scatter positions, models, sizes, turns):
// SplitMix64, integer arithmetic only, so every compiler and platform draws the same
// numbers from a seed (the standard library's distributions do not promise that). The
// float maths built on them stays the same too because the MapScript units are compiled
// without fused multiply-add (-ffp-contract=off, src/CMakeLists.txt): one rounding less
// in Range() could flip a spacing test and change every object placed after it.
class Random
{
public:
    explicit Random(std::uint64_t seed);

    std::uint64_t Next();
    // A number from 0 (included) to 1 (excluded), in steps of 2^-24.
    float Uniform();
    // A number from `low` to `high`.
    float Range(float low, float high);

private:
    std::uint64_t m_state;
};
} // namespace Editor::MapScript

#endif // _EDITOR
