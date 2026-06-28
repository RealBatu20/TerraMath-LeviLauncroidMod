// TerraMath-LeviLauncroidMod — terrain-generation hook (the binary-dependent seam).
// -----------------------------------------------------------------------------
// WHAT THIS DOES
//   TerraMath (Java) overrides terrain by replacing NoiseRouter.finalDensity
//   with a custom DensityFunction. Bedrock has no density-router; terrain is
//   produced by native column/chunk generation in libminecraftpe.so. This
//   module resolves that generation function by *signature* (regenerated from
//   the user's own binary — see docs/SIGNATURE_ANALYSIS.md) and detours it so
//   the surface height comes from FormulaEngine instead.
//
// WHY THERE IS NO HARDCODED SIGNATURE
//   The hook target's address/bytes differ per Minecraft Bedrock build and ABI.
//   Shipping a guessed pattern would be a fabricated, crash-prone offset, which
//   this project refuses to do. The signature is supplied at runtime via
//   config ("terrainSignature"). When empty, the hook stays disabled and the
//   engine/UI still run (you can preview formulas; terrain is just unchanged).
//
// THE INTEGRATION SEAM
//   `TerrainColumnHeightFn` below is the prototype the resolved function is
//   expected to match: a per-column surface-height query
//   `int f(void* self, int worldX, int worldZ)`. After IDA identifies the real
//   function, confirm its prototype matches this shape (self, x, z -> height in
//   blocks). If your target's generator exposes a different shape (e.g. a
//   density fill), swap to the density path (FormulaEngine::compute) documented
//   in HOOK_ANALYSIS.md. This is the one place that depends on the binary.
// -----------------------------------------------------------------------------
#include "Hooks.h"

#if defined(__ANDROID__)

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "../terrain/FormulaEngine.h"
#include "../util/Log.h"

// Levi Launchroid / Gloss preloader SDK (provided by the build, see CMakeLists).
#include "pl/Gloss.h"
#include "pl/Signature.h"

namespace terramath {
namespace {

constexpr int kMinWorldY = -64;   // Bedrock 1.18+ overworld floor
constexpr int kMaxWorldY = 319;   // Bedrock 1.18+ overworld ceiling

// The prototype the resolved generation function is expected to match.
using TerrainColumnHeightFn = int (*)(void* self, int worldX, int worldZ);

TerrainColumnHeightFn g_originalHeight = nullptr;

// Detour: ask the original generator (so non-overridden behaviour/biomes still
// run), then replace the height with the formula result when the engine active.
int Detour_ColumnHeight(void* self, int worldX, int worldZ) {
    int original = g_originalHeight ? g_originalHeight(self, worldX, worldZ) : 64;

    FormulaEngine& engine = FormulaEngine::instance();
    if (!engine.active()) return original;

    double y = engine.surfaceY(worldX, worldZ);
    if (y <= -1e8 || std::isnan(y) || std::isinf(y)) return original;

    int h = static_cast<int>(std::lround(y));
    return std::clamp(h, kMinWorldY, kMaxWorldY);
}

}  // namespace

bool installTerrainHook(const std::string& signature) {
    if (signature.empty()) {
        TM_LOGW(
            "terrainSignature is empty -> terrain hook disabled. The formula "
            "engine and menu still work; populate \"terrainSignature\" in the "
            "config (see docs/SIGNATURE_ANALYSIS.md) to enable world override.");
        return false;
    }

    uintptr_t addr = pl::signature::pl_resolve_signature(signature.c_str(), "libminecraftpe.so");
    if (addr == 0) {
        TM_LOGE(
            "Failed to resolve terrainSignature against libminecraftpe.so. The "
            "pattern likely does not match this Bedrock build; regenerate it "
            "for your version. Terrain hook disabled.");
        return false;
    }

    void* handle = GlossHook(reinterpret_cast<void*>(addr),
                             reinterpret_cast<void*>(&Detour_ColumnHeight),
                             reinterpret_cast<void**>(&g_originalHeight));
    if (handle == nullptr) {
        TM_LOGE("GlossHook failed to install the terrain detour at %p.",
                reinterpret_cast<void*>(addr));
        return false;
    }

    TM_LOGI("Terrain hook installed at %p (signature resolved).",
            reinterpret_cast<void*>(addr));
    return true;
}

}  // namespace terramath

#else  // non-Android host build: terrain hook is a no-op (engine is tested directly).

namespace terramath {
bool installTerrainHook(const std::string&) { return false; }
}  // namespace terramath

#endif
