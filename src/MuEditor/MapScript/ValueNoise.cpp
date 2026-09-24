#include "ValueNoise.h"

#ifdef _EDITOR

#include <cmath>

namespace Editor::MapScript
{
namespace
{
// A 32-bit integer hash of a lattice point (large odd multipliers spread neighbouring
// points apart; xor-shifts mix the high bits back down).
constexpr std::uint32_t PRIME_X = 374761393u;
constexpr std::uint32_t PRIME_Y = 668265263u;
constexpr std::uint32_t PRIME_SEED = 2246822519u;
constexpr std::uint32_t MIX = 1274126177u;
constexpr int SHIFT_1 = 13;
constexpr int SHIFT_2 = 16;
// The hash's top 24 bits make a value from -1 to 1.
constexpr int VALUE_BITS = 24;
constexpr float VALUE_SCALE = 2.0f / static_cast<float>(1u << VALUE_BITS);
// Each octave: half the lattice spacing, half the strength.
constexpr float LACUNARITY = 2.0f;
constexpr float GAIN = 0.5f;

float LatticeValue(int x, int y, std::uint32_t seed)
{
    std::uint32_t h =
        static_cast<std::uint32_t>(x) * PRIME_X + static_cast<std::uint32_t>(y) * PRIME_Y + seed * PRIME_SEED;
    h = (h ^ (h >> SHIFT_1)) * MIX;
    h ^= h >> SHIFT_2;
    return static_cast<float>(h >> (32 - VALUE_BITS)) * VALUE_SCALE - 1.0f;
}

float Smooth(float t)
{
    return t * t * (3.0f - 2.0f * t);
}

float Lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

// One octave at lattice coordinates (x, y).
float Octave(float x, float y, std::uint32_t seed)
{
    const float floorX = std::floor(x);
    const float floorY = std::floor(y);
    const int ix = static_cast<int>(floorX);
    const int iy = static_cast<int>(floorY);
    const float tx = Smooth(x - floorX);
    const float ty = Smooth(y - floorY);
    const float bottom = Lerp(LatticeValue(ix, iy, seed), LatticeValue(ix + 1, iy, seed), tx);
    const float top = Lerp(LatticeValue(ix, iy + 1, seed), LatticeValue(ix + 1, iy + 1, seed), tx);
    return Lerp(bottom, top, ty);
}
} // namespace

float FractalNoise(float x, float y, float scale, int octaves, std::uint32_t seed)
{
    if (scale <= 0.0f || octaves <= 0)
        return 0.0f;
    float sum = 0.0f;
    float strength = 1.0f;
    float totalStrength = 0.0f;
    float frequency = 1.0f / scale;
    for (int octave = 0; octave < octaves; ++octave)
    {
        // Each octave hashes with its own seed, so the octaves do not line up.
        sum += Octave(x * frequency, y * frequency, seed + static_cast<std::uint32_t>(octave)) * strength;
        totalStrength += strength;
        strength *= GAIN;
        frequency *= LACUNARITY;
    }
    return sum / totalStrength;
}
} // namespace Editor::MapScript

#endif // _EDITOR
