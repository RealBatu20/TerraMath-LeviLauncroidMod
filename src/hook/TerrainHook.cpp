// TerraMath-LeviLauncroidMod — terrain-generation hook (the binary-dependent seam).
// -----------------------------------------------------------------------------
// WHAT THIS DOES
//   TerraMath (Java) overrides terrain by replacing NoiseRouter.finalDensity
//   with a custom DensityFunction. Bedrock has no density-router; terrain is
//   produced by native column/chunk generation in libminecraftpe.so. This
//   module resolves that generation function by *signature* (regenerated from
//   the user's own binary — see docs/SIGNATURE_ANALYSIS.md) and detours it so
//   the terrain comes from FormulaEngine instead.
//
// TWO INTEGRATION SEAMS (select via config "terrainHookMode")
//   * "height"  -> the resolved function is a per-column surface-height query:
//                    int f(void* self, int worldX, int worldZ);
//                  The detour returns FormulaEngine::surfaceY(x, z).
//   * "density" -> the resolved function is a per-voxel solidity/density query:
//                    int f(void* self, int worldX, int worldY, int worldZ);
//                  The detour returns solid/air from FormulaEngine::compute.
//   Pick the seam that matches the function you identified in IDA, and adjust
//   the parameter mapping below if your build's argument order differs. This is
//   the one place that depends on the binary.
//
// WHY THERE IS NO HARDCODED SIGNATURE
//   The target's address/bytes differ per Bedrock build and ABI. Shipping a
//   guessed pattern would be a fabricated, crash-prone offset, which this
//   project refuses to do. The signature is supplied at runtime via config
//   ("terrainSignature"); when empty, the hook stays disabled and the engine/UI
//   still run.
// -----------------------------------------------------------------------------
#include "Hooks.h"

#if defined(__ANDROID__)

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>

#include "../terrain/FormulaEngine.h"
#include "../util/Log.h"
#include "AutoResolver.h"

// Levi Launchroid / Gloss preloader SDK (provided by the build, see CMakeLists).
#include "pl/Gloss.h"

namespace terramath {
namespace {

constexpr int kMinWorldY = -64;   // Bedrock 1.18+ overworld floor
constexpr int kMaxWorldY = 319;   // Bedrock 1.18+ overworld ceiling

std::atomic<bool> g_installed{false};

// --- "height" seam ----------------------------------------------------------
using ColumnHeightFn = int (*)(void* self, int worldX, int worldZ);
ColumnHeightFn g_origHeight = nullptr;

int Detour_ColumnHeight(void* self, int worldX, int worldZ) {
    int original = g_origHeight ? g_origHeight(self, worldX, worldZ) : 64;
    FormulaEngine& engine = FormulaEngine::instance();
    if (!engine.active()) return original;

    double y = engine.surfaceY(worldX, worldZ);
    if (y <= -1e8 || std::isnan(y) || std::isinf(y)) return original;
    return std::clamp(static_cast<int>(std::lround(y)), kMinWorldY, kMaxWorldY);
}

// --- "density" seam ---------------------------------------------------------
// Returns a solidity value; >0 == solid block, <=0 == air (matches the common
// Bedrock per-voxel generator shape). FormulaEngine::compute returns +1 / -1.
using VoxelDensityFn = int (*)(void* self, int worldX, int worldY, int worldZ);
VoxelDensityFn g_origDensity = nullptr;

int Detour_VoxelDensity(void* self, int worldX, int worldY, int worldZ) {
    int original = g_origDensity ? g_origDensity(self, worldX, worldY, worldZ) : 0;
    FormulaEngine& engine = FormulaEngine::instance();
    if (!engine.active()) return original;

    double d = engine.compute(worldX, worldY, worldZ);  // +1 solid / -1 air
    if (std::isnan(d) || std::isinf(d)) return original;
    return d >= 0.0 ? 1 : -1;
}

bool installAt(uintptr_t addr, const std::string& mode) {
    void* handle = nullptr;
    if (mode == "density") {
        handle = GlossHook(reinterpret_cast<void*>(addr),
                           reinterpret_cast<void*>(&Detour_VoxelDensity),
                           reinterpret_cast<void**>(&g_origDensity));
    } else {  // default "height"
        handle = GlossHook(reinterpret_cast<void*>(addr),
                           reinterpret_cast<void*>(&Detour_ColumnHeight),
                           reinterpret_cast<void**>(&g_origHeight));
    }
    if (handle == nullptr) {
        TM_LOGE("GlossHook failed to install the terrain detour at %p (mode=%s).",
                reinterpret_cast<void*>(addr), mode.c_str());
        return false;
    }
    return true;
}

}  // namespace

bool terrainHookInstalled() { return g_installed.load(std::memory_order_acquire); }

bool installTerrainHook(const std::string& signature, const std::string& mode) {
    if (g_installed.load(std::memory_order_acquire)) {
        TM_LOGW("Terrain hook already installed; ignoring re-install request.");
        return false;
    }
    if (signature.empty()) {
        TM_LOGW(
            "terrainSignature is empty -> terrain hook disabled. The formula "
            "engine and menu still work; populate \"terrainSignature\" in the "
            "config (see docs/SIGNATURE_ANALYSIS.md) to enable world override.");
        return false;
    }

    uintptr_t addr = resolveSignature(signature);
    if (addr == 0) {
        TM_LOGE(
            "Failed to resolve terrainSignature against libminecraftpe.so. The "
            "pattern likely does not match this Bedrock build; regenerate it "
            "for your version. Terrain hook disabled.");
        return false;
    }

    if (!installAt(addr, mode)) return false;

    g_installed.store(true, std::memory_order_release);
    TM_LOGI("Terrain hook installed at %p (mode=%s, signature resolved).",
            reinterpret_cast<void*>(addr), mode.c_str());
    return true;
}

bool installTerrainHookAuto(const ResolverConfig& cfg, std::string& outStatus) {
    if (g_installed.load(std::memory_order_acquire)) {
        outStatus = "Already installed.";
        return false;
    }

    // 1) Auto-detect by resolving the generation function from the binary.
    if (cfg.autoDetect) {
        ResolveResult r = autoResolveTerrainFunction(cfg);
        if (r.ok && installAt(r.address, cfg.hookMode)) {
            g_installed.store(true, std::memory_order_release);
            outStatus = "Auto-detected & hooked (" + r.matchedName + ").";
            TM_LOGI("Terrain hook auto-installed at %p (mode=%s).",
                    reinterpret_cast<void*>(r.address), cfg.hookMode.c_str());
            return true;
        }
    }

    // 2) Fallback: a manual signature, only if one was deliberately provided.
    if (!cfg.manualSignature.empty()) {
        uintptr_t addr = resolveSignature(cfg.manualSignature);
        if (addr != 0 && installAt(addr, cfg.hookMode)) {
            g_installed.store(true, std::memory_order_release);
            outStatus = "Hooked via manual signature.";
            return true;
        }
        outStatus = "Manual signature did not resolve.";
        return false;
    }

    outStatus = cfg.autoDetect ? "Auto-detect found no match (adjust hints in Advanced)."
                               : "Auto-detect off and no manual signature set.";
    return false;
}

}  // namespace terramath

#else  // non-Android host build: terrain hook is a no-op (engine is tested directly).

namespace terramath {
bool installTerrainHook(const std::string&, const std::string&) { return false; }
bool installTerrainHookAuto(const ResolverConfig&, std::string& s) {
    s = "host build";
    return false;
}
bool terrainHookInstalled() { return false; }
}  // namespace terramath

#endif
