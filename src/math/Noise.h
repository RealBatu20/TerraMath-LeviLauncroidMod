// TerraMath-LeviLauncroidMod
// -----------------------------------------------------------------------------
// Coherent-noise provider — native replacement for TerraMath's CompositeNoise.
//
// IMPORTANT (feature-parity note): TerraMath's CompositeNoise wraps Minecraft
// *Java*'s internal PerlinNoise / SimplexNoise / NormalNoise / BlendedNoise
// classes. Those are seed- and version-specific and are NOT exposed on Bedrock.
// This module provides standard, self-contained, seeded implementations:
//   - perlin  -> classic 3D improved Perlin noise, range ~[-1, 1]
//   - simplex -> 2D simplex noise, range ~[-1, 1]
//   - octaved -> fractal sum of Perlin octaves
//   - normal  -> 2-octave Perlin composite (approximates NormalNoise's shape)
//   - blended -> low-frequency Perlin composite (approximates BlendedNoise)
//
// These are coherent and seed-stable but NOT bit-identical to Java MC noise.
// See docs/CONVERSION_REPORT.md (FACT/INFERENCE/HYPOTHESIS) for the rationale.
// -----------------------------------------------------------------------------
#pragma once

#include <array>
#include <cstdint>

namespace terramath::math {

class Noise {
public:
    explicit Noise(uint64_t seed);

    double perlin(double x, double y, double z) const;
    double simplex(double x, double z) const;
    double octaved(double x, double z, int octaves, double persistence) const;
    double normal(double x, double y, double z) const;
    double blended(double x, double y, double z) const;

private:
    std::array<int, 512> perm_{};   // doubled permutation table
    std::array<int, 512> permSimp_{};

    static double fade(double t);
    static double lerp(double t, double a, double b);
    static double grad(int hash, double x, double y, double z);
};

}  // namespace terramath::math
