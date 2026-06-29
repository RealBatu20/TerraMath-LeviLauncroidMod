// TerraMath-LeviLauncroidMod — hook entry points.
#pragma once

#include <string>

namespace terramath {

// Path to the mod's config/data directory (created under the game's files dir).
extern std::string g_dataDir;

// Installs the ImGui render hook (libEGL eglSwapBuffers) + the touch input hook.
// Returns true if the render hook installed; the UI works even if the terrain
// hook is disabled.
bool installUiHooks();

// Installs the terrain-generation hook in libminecraftpe.so using the signature
// from config. `mode` selects the detour shape: "height" (column surface) or
// "density" (per-voxel solidity). Returns false (and logs) if the signature is
// empty/unresolved or already installed — in that case the engine + UI still
// run, but generated terrain is unchanged.
bool installTerrainHook(const std::string& signature, const std::string& mode = "height");

// True once a terrain hook has been successfully installed this session.
bool terrainHookInstalled();

}  // namespace terramath
