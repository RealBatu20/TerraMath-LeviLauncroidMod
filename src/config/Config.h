// TerraMath-LeviLauncroidMod
// -----------------------------------------------------------------------------
// Config — native port of ModConfig (Java). Persists the formula, terrain
// parameters and noise settings as JSON, plus one extra Android-only field:
//
//   "terrainSignature": the IDA byte-pattern of Bedrock's terrain-generation
//   function for the *user's* libminecraftpe.so. This is the single
//   binary-specific value that cannot be derived without the target binary; it
//   is sourced from config (or a compile define) instead of being hardcoded /
//   invented. See docs/SIGNATURE_ANALYSIS.md for how to regenerate it.
//
// Dependency-free JSON (flat schema we fully control) so it also builds & runs
// on the host for testing.
// -----------------------------------------------------------------------------
#pragma once

#include <string>

#include "../hook/ResolverConfig.h"
#include "../terrain/TerrainSettings.h"

namespace terramath {

struct Config {
    TerrainSettings terrain;
    bool useDefaultFormula = true;
    std::string baseFormula = "";

    // Terrain-hook target resolution. Auto-detect is on by default, so the mod
    // finds the generation function itself — no byte signature to paste. All of
    // this is editable in the menu's Advanced section.
    ResolverConfig resolver;

    // Load from path; returns defaults if the file is missing/unreadable.
    static Config load(const std::string& path);
    // Atomically write to path. Returns false on I/O error.
    bool save(const std::string& path) const;

    std::string toJson() const;
    static Config fromJson(const std::string& json);
};

}  // namespace terramath
