#pragma once

#ifdef _EDITOR

#include <cstdint>

namespace Editor::MapScript
{
// Smooth random bumps for terrain.noise, the same for a seed on every platform.
//
// Value noise: every point of a lattice `scale` tiles apart gets a random value from an
// integer hash of its coordinates and the seed; between lattice points the values are
// blended with a smoothstep, so the surface has no creases. Each further octave adds
// the same noise at half the spacing and half the strength (fine detail on top of the
// broad shape). The sum is divided by the total strength, so the result stays within
// -1 to 1 whatever the octave count.
float FractalNoise(float x, float y, float scale, int octaves, std::uint32_t seed);
} // namespace Editor::MapScript

#endif // _EDITOR
