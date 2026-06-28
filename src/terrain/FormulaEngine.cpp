#include "FormulaEngine.h"

#include <algorithm>
#include <cmath>

#include "../math/MathFunctions.h"

namespace terramath {

const char* noiseTypeName(NoiseType t) {
    switch (t) {
        case NoiseType::None: return "NONE";
        case NoiseType::Perlin: return "PERLIN";
        case NoiseType::Simplex: return "SIMPLEX";
        case NoiseType::Blended: return "BLENDED";
        case NoiseType::Normal: return "NORMAL";
    }
    return "NONE";
}

NoiseType noiseTypeFromName(const std::string& s) {
    if (s == "PERLIN") return NoiseType::Perlin;
    if (s == "SIMPLEX") return NoiseType::Simplex;
    if (s == "BLENDED") return NoiseType::Blended;
    if (s == "NORMAL") return NoiseType::Normal;
    return NoiseType::None;
}

FormulaEngine& FormulaEngine::instance() {
    static FormulaEngine inst;
    return inst;
}

bool FormulaEngine::apply(const TerrainSettings& s, std::string& error) {
    // Empty formula => deactivate (vanilla terrain), as in NoiseRouterMixin.
    std::string trimmed = s.formula;
    trimmed.erase(0, trimmed.find_first_not_of(" \t\r\n"));
    if (trimmed.find_last_not_of(" \t\r\n") != std::string::npos)
        trimmed.erase(trimmed.find_last_not_of(" \t\r\n") + 1);

    if (trimmed.empty()) {
        std::lock_guard<std::mutex> lk(mtx_);
        current_.reset();
        active_.store(false, std::memory_order_release);
        return true;
    }

    auto compiled = std::make_shared<Compiled>();
    compiled->settings = s;
    try {
        compiled->ast = math::parseFormula(s.formula, compiled->isEquation);
    } catch (const math::FormulaError& e) {
        error = e.what();
        return false;  // keep previous formula active
    }
    compiled->noise = std::make_unique<math::Noise>(s.seed);
    math::seedRng(s.seed);

    {
        std::lock_guard<std::mutex> lk(mtx_);
        current_ = compiled;
        active_.store(true, std::memory_order_release);
    }
    error.clear();
    return true;
}

TerrainSettings FormulaEngine::settings() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return current_ ? current_->settings : TerrainSettings{};
}

double FormulaEngine::evalWithOverlay(const Compiled& c, double sx, double sy,
                                      double sz) const {
    math::EvalContext ctx;
    ctx.x = sx;
    ctx.y = sy;
    ctx.z = sz;
    ctx.noise = c.noise.get();

    double base = c.ast->eval(ctx);

    const TerrainSettings& s = c.settings;
    if (s.noiseType == NoiseType::None) return base;

    double nx = sx / s.noiseScaleX;
    double ny = sy / s.noiseScaleY;
    double nz = sz / s.noiseScaleZ;
    double nv = 0.0;
    switch (s.noiseType) {
        case NoiseType::Perlin: nv = c.noise->perlin(nx, ny, nz); break;
        case NoiseType::Simplex: nv = c.noise->simplex(nx, nz); break;
        case NoiseType::Blended: nv = c.noise->blended(nx, ny, nz); break;
        case NoiseType::Normal: nv = c.noise->normal(nx, ny, nz); break;
        case NoiseType::None: break;
    }
    return base + nv * s.noiseHeightScale;
}

double FormulaEngine::spatialSmooth(const Compiled& c, double sx, double sy,
                                    double sz) const {
    double sf = c.settings.smoothingFactor;
    if (c.isEquation) {
        return (evalWithOverlay(c, sx, sy, sz) + evalWithOverlay(c, sx + sf, sy, sz) +
                evalWithOverlay(c, sx - sf, sy, sz) + evalWithOverlay(c, sx, sy + sf, sz) +
                evalWithOverlay(c, sx, sy - sf, sz) + evalWithOverlay(c, sx, sy, sz + sf) +
                evalWithOverlay(c, sx, sy, sz - sf)) /
               7.0;
    }
    return (evalWithOverlay(c, sx, sy, sz) + evalWithOverlay(c, sx + sf, sy, sz) +
            evalWithOverlay(c, sx - sf, sy, sz) + evalWithOverlay(c, sx, sy, sz + sf) +
            evalWithOverlay(c, sx, sy, sz - sf)) /
           5.0;
}

namespace {
double computeDensity(double fv, double sy, double scale, bool eq, bool dm,
                      double baseHeight, double heightVariation) {
    double d;
    if (eq) {
        d = dm ? (fv - (baseHeight - 64.0) / scale) / std::max(1e-6, heightVariation) : fv;
    } else {
        double tgt = dm ? baseHeight / scale + fv * heightVariation : 64.0 / scale + fv;
        d = tgt - sy;
    }
    return d >= 0.0 ? 1.0 : -1.0;
}
}  // namespace

double FormulaEngine::compute(int blockX, int blockY, int blockZ) const {
    std::shared_ptr<const Compiled> c;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        c = current_;
    }
    if (!c) return 0.0;
    try {
        const TerrainSettings& s = c->settings;
        double scale = s.coordinateScale;
        double sx = blockX / scale, sy = blockY / scale, sz = blockZ / scale;
        double fv = s.smoothingFactor > 0.0 ? spatialSmooth(*c, sx, sy, sz)
                                            : evalWithOverlay(*c, sx, sy, sz);
        double d = computeDensity(fv, sy, scale, c->isEquation, s.useDensityMode,
                                  s.baseHeight, s.heightVariation);
        if (std::isnan(d) || std::isinf(d)) return 0.0;
        return d;
    } catch (...) {
        return 0.0;
    }
}

double FormulaEngine::surfaceY(int blockX, int blockZ) const {
    std::shared_ptr<const Compiled> c;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        c = current_;
    }
    if (!c) return -1e9;
    try {
        const TerrainSettings& s = c->settings;
        double scale = s.coordinateScale;
        double sx = blockX / scale, sz = blockZ / scale;

        if (!c->isEquation) {
            // Closed-form column height (formula's y term sampled at y = 0).
            double fv = s.smoothingFactor > 0.0 ? spatialSmooth(*c, sx, 0.0, sz)
                                                : evalWithOverlay(*c, sx, 0.0, sz);
            double y = s.useDensityMode ? s.baseHeight + fv * s.heightVariation * scale
                                        : 64.0 + fv * scale;
            if (std::isnan(y) || std::isinf(y)) return -1e9;
            return y;
        }

        // Equation mode: 3D iso-surface — scan top-down for the first solid cell.
        for (int by = 319; by >= -64; --by) {
            if (compute(blockX, by, blockZ) >= 0.5) return static_cast<double>(by);
        }
        return -64.0;
    } catch (...) {
        return -1e9;
    }
}

}  // namespace terramath
