// TerraMath-LeviLauncroidMod
// -----------------------------------------------------------------------------
// Curated formula presets — the "pick one and go" UX. Each preset bundles a
// formula with sensible terrain parameters so a tap produces good-looking
// terrain without fiddling. Pure data, no Android deps (host-testable).
// -----------------------------------------------------------------------------
#pragma once

#include <array>
#include <cstddef>

#include "TerrainSettings.h"

namespace terramath {

struct Preset {
    const char* name;
    const char* formula;
    double coordinateScale;
    double baseHeight;
    double heightVariation;
    double smoothingFactor;
    bool   useDensityMode;
    NoiseType noiseType;
    double noiseHeightScale;
};

// Applies a preset onto a TerrainSettings (keeps seed + noise scales).
inline void applyPreset(const Preset& p, TerrainSettings& s) {
    s.formula = p.formula;
    s.coordinateScale = p.coordinateScale;
    s.baseHeight = p.baseHeight;
    s.heightVariation = p.heightVariation;
    s.smoothingFactor = p.smoothingFactor;
    s.useDensityMode = p.useDensityMode;
    s.noiseType = p.noiseType;
    s.noiseHeightScale = p.noiseHeightScale;
}

// Default-ish noise scales are taken from TerrainSettings defaults (30/30/30).
inline const std::array<Preset, 12>& presets() {
    static const std::array<Preset, 12> kPresets = {{
        {"Flat plains",        "0",                                 5.0, 64.0, 16.0, 0.50, false, NoiseType::Perlin, 3.0},
        {"Rolling waves",      "sin(x) * cos(z) * 10",              5.0, 64.0, 32.5, 0.55, false, NoiseType::None,   4.0},
        {"Broad swells",       "sin(x/3)*10 + cos(z/3)*10",         6.0, 70.0, 32.5, 0.60, false, NoiseType::None,   4.0},
        {"Smooth hills",       "perlin(x, 0, z) * 20",              5.0, 72.0, 30.0, 0.55, false, NoiseType::None,   4.0},
        {"Rugged mountains",   "perlin(x,0,z)*40 + perlin(x*2,0,z*2)*15", 4.0, 80.0, 60.0, 0.45, false, NoiseType::None, 6.0},
        {"Conical bowl",       "sqrt(x*x + z*z)",                   8.0, 50.0, 20.0, 0.55, false, NoiseType::None,   4.0},
        {"Ripple crater",      "sin(sqrt(x*x+z*z)) * 8",            5.0, 70.0, 24.0, 0.55, false, NoiseType::Perlin, 3.0},
        {"Floating islands",   "y = perlin(x,y,z) - 0.2",           4.0, 96.0, 32.5, 0.55, true,  NoiseType::None,   4.0},
        {"Spiky terrain",      "abs(sin(x)) * abs(cos(z)) * 30",    5.0, 64.0, 30.0, 0.40, false, NoiseType::None,   4.0},
        {"Terraces",           "floor(perlin(x,0,z) * 6) * 6",      5.0, 70.0, 30.0, 0.30, false, NoiseType::None,   4.0},
        {"Dunes",              "sin(x/2) * 12 + perlin(x,0,z)*6",   5.0, 66.0, 28.0, 0.60, false, NoiseType::Perlin, 4.0},
        {"Canyon walls",       "tan(clamp(sin(x/4), -0.9, 0.9)) * 8", 5.0, 64.0, 30.0, 0.55, false, NoiseType::None, 4.0},
    }};
    return kPresets;
}

}  // namespace terramath
