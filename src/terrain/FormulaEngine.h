// TerraMath-LeviLauncroidMod
// -----------------------------------------------------------------------------
// FormulaEngine — native port of TerraMathDensityFunction (Java).
//
// Owns the compiled formula + noise + settings and exposes:
//   - compute(...)    : density-sign value, identical math to the Java
//                       DensityFunction.compute() (for a density-based hook).
//   - surfaceY(x, z)  : terrain surface height for a column (for a heightmap-
//                       based Bedrock hook — the more natural Bedrock path).
//
// Thread-safety: set()/parse happens on the UI thread; compute()/surfaceY()
// are read-only after a successful set() and safe to call from the generation
// thread. The active formula pointer is swapped atomically.
// -----------------------------------------------------------------------------
#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>

#include "../math/Expr.h"
#include "../math/Noise.h"
#include "TerrainSettings.h"

namespace terramath {

class FormulaEngine {
public:
    static FormulaEngine& instance();

    // (Re)compile from settings. Returns false + fills error on parse failure;
    // on failure the previously active formula (if any) is left untouched.
    bool apply(const TerrainSettings& s, std::string& error);

    bool active() const { return active_.load(std::memory_order_acquire); }

    // Density-sign value at a block position (Java parity path). Returns 0 if
    // the engine is inactive. Never throws.
    double compute(int blockX, int blockY, int blockZ) const;

    // Surface height for a column (heightmap hook path). Returns INT_MIN-ish
    // sentinel (-1e9) if inactive. Never throws.
    double surfaceY(int blockX, int blockZ) const;

    TerrainSettings settings() const;

private:
    FormulaEngine() = default;

    struct Compiled {
        std::unique_ptr<math::Node> ast;
        std::unique_ptr<math::Noise> noise;
        TerrainSettings settings;
        bool isEquation = false;
    };

    double evalWithOverlay(const Compiled& c, double sx, double sy, double sz) const;
    double spatialSmooth(const Compiled& c, double sx, double sy, double sz) const;

    mutable std::mutex mtx_;
    std::shared_ptr<const Compiled> current_;
    std::atomic<bool> active_{false};
};

}  // namespace terramath
