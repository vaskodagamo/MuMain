#include "ScriptRandom.h"

#ifdef _EDITOR

namespace Editor::MapScript
{
namespace
{
// SplitMix64 (Steele, Lea and Flood, 2014): the golden-ratio increment and the two
// multipliers of its finaliser.
constexpr std::uint64_t GOLDEN_GAMMA = 0x9E3779B97F4A7C15ull;
constexpr std::uint64_t MIX_1 = 0xBF58476D1CE4E5B9ull;
constexpr std::uint64_t MIX_2 = 0x94D049BB133111EBull;
constexpr int SHIFT_1 = 30;
constexpr int SHIFT_2 = 27;
constexpr int SHIFT_3 = 31;

// The top 24 bits make a float in [0, 1) without rounding up to 1.
constexpr int FLOAT_BITS = 24;
constexpr int DROPPED_BITS = 64 - FLOAT_BITS;
constexpr float FLOAT_STEP = 1.0f / static_cast<float>(1u << FLOAT_BITS);
} // namespace

Random::Random(std::uint64_t seed) : m_state(seed) {}

std::uint64_t Random::Next()
{
    m_state += GOLDEN_GAMMA;
    std::uint64_t z = m_state;
    z = (z ^ (z >> SHIFT_1)) * MIX_1;
    z = (z ^ (z >> SHIFT_2)) * MIX_2;
    return z ^ (z >> SHIFT_3);
}

float Random::Uniform()
{
    return static_cast<float>(Next() >> DROPPED_BITS) * FLOAT_STEP;
}

float Random::Range(float low, float high)
{
    return low + (high - low) * Uniform();
}
} // namespace Editor::MapScript

#endif // _EDITOR
