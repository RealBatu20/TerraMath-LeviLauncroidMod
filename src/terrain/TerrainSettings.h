// TerraMath-LeviLauncroidMod
// -----------------------------------------------------------------------------
// Terrain parameters — native port of TerrainSettingsManager + ModConfig fields.
// Defaults match TerrainSettingsManager.getDefaultByType (Java).
// -----------------------------------------------------------------------------
#pragma once

#include <cstdint>
#include <string>

namespace terramath {

enum class NoiseType { None, Perlin, Simplex, Blended, Normal };

const char* noiseTypeName(NoiseType t);
NoiseType   noiseTypeFromName(const std::string& s);

struct TerrainSettings {
    std::string formula = "";          // empty -> mod inactive (vanilla terrain)
    double coordinateScale = 5.0;      // COORDINATE_SCALE
    double baseHeight = 64.0;          // BASE_HEIGHT
    double heightVariation = 32.5;     // HEIGHT_VARIATION
    double smoothingFactor = 0.55;     // SMOOTHING_FACTOR
    bool   useDensityMode = false;

    NoiseType noiseType = NoiseType::None;
    double noiseScaleX = 30.0;
    double noiseScaleY = 30.0;
    double noiseScaleZ = 30.0;
    double noiseHeightScale = 4.0;

    uint64_t seed = 0;                 // world seed (drives noise + rand functions)

    void resetToDefaults() { *this = TerrainSettings{}; }
};

}  // namespace terramath
