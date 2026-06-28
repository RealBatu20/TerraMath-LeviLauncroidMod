// TerraMath-LeviLauncroidMod — host unit tests for the portable formula engine.
// Build & run: see build.sh `--tests` or CMake target `terramath_tests`.
//
// These tests run on the host (no Android / NDK needed) and validate that the
// native parser/evaluator reproduces the documented TerraMath semantics.
#include <cmath>
#include <cstdio>
#include <string>

#include "../src/config/Config.h"
#include "../src/math/Expr.h"
#include "../src/math/MathFunctions.h"
#include "../src/math/Noise.h"
#include "../src/terrain/FormulaEngine.h"
#include "../src/terrain/TerrainSettings.h"

using namespace terramath;
using namespace terramath::math;

static int g_failures = 0;
static int g_checks = 0;

static void check(bool cond, const std::string& msg) {
    ++g_checks;
    if (!cond) {
        ++g_failures;
        std::printf("  [FAIL] %s\n", msg.c_str());
    }
}

static void approx(double got, double want, const std::string& msg, double eps = 1e-6) {
    ++g_checks;
    if (std::fabs(got - want) > eps) {
        ++g_failures;
        std::printf("  [FAIL] %s (got %.9g, want %.9g)\n", msg.c_str(), got, want);
    }
}

static double evalNo(const std::string& f, double x, double y, double z) {
    bool eq = false;
    auto ast = parseFormula(f, eq);
    EvalContext c;
    c.x = x;
    c.y = y;
    c.z = z;
    return ast->eval(c);
}

int main() {
    std::printf("TerraMath formula-engine tests\n");

    // --- arithmetic & precedence --------------------------------------------
    approx(evalNo("1 + 2 * 3", 0, 0, 0), 7.0, "precedence * over +");
    approx(evalNo("(1 + 2) * 3", 0, 0, 0), 9.0, "parentheses");
    approx(evalNo("2 ^ 3 ^ 2", 0, 0, 0), 512.0, "power right-assoc 2^(3^2)");
    approx(evalNo("-2 ^ 2", 0, 0, 0), -4.0, "unary minus below power: -(2^2)");
    approx(evalNo("2 ^ -2", 0, 0, 0), 0.25, "negative exponent");
    approx(evalNo("7 % 3", 0, 0, 0), 1.0, "modulo operator");

    // --- variables ----------------------------------------------------------
    approx(evalNo("x + y + z", 1, 2, 3), 6.0, "variables x,y,z");
    approx(evalNo("sin(x) * cos(z) * 10", M_PI / 2, 0, 0), 10.0, "sin*cos sample");

    // --- constants ----------------------------------------------------------
    approx(evalNo("pi", 0, 0, 0), M_PI, "pi");
    approx(evalNo("e", 0, 0, 0), M_E, "euler e as constant");
    approx(evalNo("phi", 0, 0, 0), 1.618033988749894848204, "phi");
    approx(evalNo("2 * pi", 0, 0, 0), 2 * M_PI, "2*pi");

    // --- scientific notation vs constant e ----------------------------------
    approx(evalNo("1e3", 0, 0, 0), 1000.0, "scientific 1e3");
    approx(evalNo("2.5e-2", 0, 0, 0), 0.025, "scientific 2.5e-2");

    // --- functions ----------------------------------------------------------
    approx(evalNo("sqrt(16)", 0, 0, 0), 4.0, "sqrt");
    approx(evalNo("max(3, 7)", 0, 0, 0), 7.0, "max");
    approx(evalNo("clamp(15, 0, 10)", 0, 0, 0), 10.0, "clamp 3-arg");
    approx(evalNo("root(27, 3)", 0, 0, 0), 3.0, "root", 1e-9);
    approx(evalNo("abs(-5)", 0, 0, 0), 5.0, "abs");
    approx(evalNo("ln(e)", 0, 0, 0), 1.0, "ln(e)");
    approx(evalNo("lg(1000)", 0, 0, 0), 3.0, "lg base-10", 1e-9);
    approx(evalNo("sign(-3)", 0, 0, 0), -1.0, "sign");
    approx(evalNo("mod(7, 3)", 0, 0, 0), 1.0, "mod()");

    // gamma: gamma(5) = 4! = 24
    approx(evalNo("gamma(5)", 0, 0, 0), 24.0, "gamma(5)=24", 1e-6);
    // factorial postfix: 4! = 24 (via gamma(n+1))
    approx(evalNo("4!", 0, 0, 0), 24.0, "postfix factorial 4!", 1e-6);

    // erf(0)=0, erf large ~ 1
    approx(evalNo("erf(0)", 0, 0, 0), 0.0, "erf(0)", 1e-6);
    check(std::fabs(evalNo("erf(3)", 0, 0, 0) - 1.0) < 1e-3, "erf(3) ~ 1");

    // --- equation mode ------------------------------------------------------
    {
        bool eq = false;
        auto ast = parseFormula("y = sin(x)", eq);
        check(eq, "equation mode detected for 'y = sin(x)'");
        EvalContext c;
        c.x = 0;
        c.y = 0.5;
        c.z = 0;
        // (rhs) - (lhs) = sin(0) - 0.5 = -0.5
        approx(ast->eval(c), -0.5, "equation rewrite (rhs)-(lhs)");
    }

    // --- error handling -----------------------------------------------------
    {
        bool threw = false;
        try {
            bool eq = false;
            parseFormula("sin(", eq);
        } catch (const FormulaError&) {
            threw = true;
        }
        check(threw, "malformed 'sin(' throws FormulaError");
    }
    {
        bool threw = false;
        try {
            bool eq = false;
            parseFormula("foobar(x)", eq);
        } catch (const FormulaError&) {
            threw = true;
        }
        check(threw, "unknown function throws");
    }
    {
        bool threw = false;
        try {
            bool eq = false;
            parseFormula("max(1)", eq);  // wrong arity
        } catch (const FormulaError&) {
            threw = true;
        }
        check(threw, "wrong arity throws");
    }

    // --- noise determinism --------------------------------------------------
    {
        Noise a(12345), b(12345), c(999);
        double va = a.perlin(1.5, 2.5, 3.5);
        double vb = b.perlin(1.5, 2.5, 3.5);
        approx(va, vb, "perlin deterministic for equal seed");
        check(va >= -1.5 && va <= 1.5, "perlin within bounds");
        // Different seeds must produce a different noise field overall (a single
        // lattice point can coincide, so compare across several samples).
        double diff = 0.0;
        for (int i = 0; i < 8; ++i) {
            double p = 0.37 * i + 0.13;
            diff += std::fabs(a.perlin(p, p * 1.7, p * 0.5) - c.perlin(p, p * 1.7, p * 0.5));
        }
        check(diff > 1e-6, "perlin field differs for different seed");
        double s = a.simplex(0.3, 0.7);
        check(s >= -1.5 && s <= 1.5, "simplex within bounds");
    }

    // --- FormulaEngine end-to-end -------------------------------------------
    {
        TerrainSettings s;
        s.formula = "sin(x) * cos(z) * 10";
        s.seed = 42;
        std::string err;
        bool ok = FormulaEngine::instance().apply(s, err);
        check(ok, "engine.apply succeeds: " + err);
        check(FormulaEngine::instance().active(), "engine active after apply");

        double y0 = FormulaEngine::instance().surfaceY(0, 0);
        // x=0,z=0 -> sin(0)*cos(0)*10 = 0, fv=0 (smoothing of flat 0 region ~0)
        // surface y = 64 + fv*scale ; fv near 0 -> ~64
        check(y0 > 40.0 && y0 < 90.0, "surfaceY near base height at origin");

        // density sign check: well below surface should be solid (>=0),
        // well above should be air (<0) for a height-style formula.
        double below = FormulaEngine::instance().compute(0, 0, 0);
        double above = FormulaEngine::instance().compute(0, 300, 0);
        check(below >= 0.0, "solid below surface");
        check(above < 0.0, "air high above surface");
    }

    // --- empty formula deactivates ------------------------------------------
    {
        TerrainSettings s;
        s.formula = "   ";
        std::string err;
        FormulaEngine::instance().apply(s, err);
        check(!FormulaEngine::instance().active(), "empty formula deactivates engine");
    }

    // --- config round-trip --------------------------------------------------
    {
        Config c;
        c.terrain.formula = "sin(x) * cos(z) * 10";
        c.terrain.coordinateScale = 7.5;
        c.terrain.noiseType = NoiseType::Perlin;
        c.terrain.useDensityMode = true;
        c.terrainSignature = "DE AD BE EF ?? 90";
        c.terrainHookMode = "density";
        Config r = Config::fromJson(c.toJson());
        check(r.terrain.formula == c.terrain.formula, "config formula round-trips");
        approx(r.terrain.coordinateScale, 7.5, "config scale round-trips");
        check(r.terrain.noiseType == NoiseType::Perlin, "config noiseType round-trips");
        check(r.terrain.useDensityMode, "config bool round-trips");
        check(r.terrainSignature == "DE AD BE EF ?? 90", "config signature round-trips");
        check(r.terrainHookMode == "density", "config hook mode round-trips");
    }

    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
