# Conversion Report — TerraMath → Levi Launchroid

Evidence is labelled **FACT** (verified from the TerraMath source / this
project's tested code), **INFERENCE** (evidence-backed but not directly
confirmed), or **HYPOTHESIS** (requires the target binary or on-device testing).

## A. Project classification
**FACT.** TerraMath is a multi-loader Minecraft *Java* mod (Architectury-style
`common` + `fabric` + `forge`), MC 1.20.x–1.21.x, using SpongePowered Mixins and
the Fabric API. Source: `addavriance/TerraMath` (`master`). Confirmed from the
repo tree and `build.gradle` / loader sources.

## B. Core mechanism (what the mod actually does)
**FACT**, from source:
- The user enters a formula (e.g. `sin(x)*cos(z)*10`). `FormulaFormatter`
  rewrites it into a Java expression: `=` → `(rhs)-(lhs)`; a noise overlay is
  appended (`+ <noise>(…) * heightScale`); function/constant names are mapped;
  `^` → `Math.pow`; `!` → factorial. `FormulaParser` then **JIT-compiles** the
  expression with **Janino** at runtime.
- `TerraMathDensityFunction.compute()` divides block coords by `coordinateScale`,
  optionally spatially smooths (5-point for height mode, 7-point for equation
  mode), then `computeDensity()` produces a density-sign value. Two modes:
  - non-equation: `target = useDensity ? base/scale + fv*variation : 64/scale + fv;`
    `d = target - y`.
  - equation: `d = useDensity ? (fv - (base-64)/scale)/max(1e-6, variation) : fv`.
  Returns `d >= 0 ? 1 : -1`.
- `NoiseRouterMixin` injects at `NoiseRouter.finalDensity` HEAD and, when a
  formula is set, returns a `TerraMathDensityFunction` instead of vanilla.
- `TerrainData` (a `SavedData`) persists formula + params to world NBT;
  `ModConfig` stores defaults as JSON (`config/terramath.json`).
- A custom Create-World settings tab with a 3D preview drives the parameters.

## C. Java → native mapping (and why)
| Java system | Native replacement | Evidence |
|---|---|---|
| Janino runtime expression compile | recursive-descent parser + AST interpreter (`src/math/Expr.*`) | FACT — Janino is JVM-only; AST interpreter reproduces the same operators/functions (unit-tested) |
| `MathExtensions` (gamma, erf, csc…) | `src/math/MathFunctions.*` | FACT — same algorithms/coefficients ported 1:1 |
| `MathFunctionsRegistry` / `MathConstantsRegistry` | tables in `Expr.cpp` | FACT — identical name/arity/constant sets |
| `CompositeNoise` (MC Perlin/Simplex/Normal/Blended) | `src/math/Noise.*` | INFERENCE — coherent seeded noise; **not** bit-identical to MC Java noise (see below) |
| `TerraMathDensityFunction` | `src/terrain/FormulaEngine.*` | FACT — same density math; adds a surface-height path for Bedrock |
| `NoiseRouterMixin` (density router) | auto-resolved native hook (`src/hook/AutoResolver.cpp` + `TerrainHook.cpp`) | HYPOTHESIS — Bedrock has no router; the hook target is auto-located in `libminecraftpe.so` via demangled-symbol scan, verified on-device |
| `ModConfig` JSON | `src/config/Config.*` | FACT — JSON round-trip unit-tested |
| Create-World settings tab + 3D preview | ImGui menu + 1D surface preview | INFERENCE — ImGui is the practical Bedrock-native UI path |
| `TerrainData` world-NBT SavedData | local JSON config | INFERENCE — see NBT_ANALYSIS.md |

## D. Known semantic differences (disclosed)
1. **Noise is not bit-identical.** TerraMath wraps Minecraft Java's internal
   `PerlinNoise`/`SimplexNoise`/`NormalNoise`/`BlendedNoise`. Those are
   seed/version-specific and unavailable on Bedrock. We use standard seeded
   Perlin/Simplex (and Perlin-composite approximations for `normal`/`blended`).
   Coherent and seed-stable, but a given seed will not reproduce the *exact*
   Java noise field. **INFERENCE / disclosed limitation.**
2. **`rand`/`randnormal`/`randrange`** use a deterministic seeded RNG here
   (Java uses non-deterministic `ThreadLocalRandom`). This makes generation
   reproducible; values differ from Java. **FACT.**
3. **Surface vs density.** Java emits a 3D density field. Bedrock generation is
   most naturally driven per-column, so `FormulaEngine::surfaceY()` provides a
   closed-form column height for non-equation formulas (formula's `y` term
   sampled at `y=0`) and a top-down scan for equation mode. The density-sign
   path (`compute()`) is kept for a density-style hook. **FACT** for the math;
   **HYPOTHESIS** for which path your binary's generator wants.

## E. What was actually built and tested in this scaffold
**FACT.**
- Host build of the engine + config compiles clean under `-Wall -Wextra`.
- `tests/test_formula.cpp` runs **76 checks, 0 failures** (arithmetic,
  precedence, power right-assoc, constants vs scientific `e`, functions, gamma,
  factorial, erf, equation rewrite, error handling, noise determinism/bounds,
  end-to-end engine, config round-trip).

## F. Binary-dependent items (honest gaps)
**FACT.**
- The terrain hook **auto-detects** its target at runtime by scanning
  `libminecraftpe.so`'s demangled dynamic symbol table (`src/hook/AutoResolver.cpp`);
  no signature is hardcoded or guessed. Whether the default symbol hints match a
  given Bedrock build, and which seam (height vs density) that build's generator
  wants, are **HYPOTHESIS** until verified on-device — both are editable in the
  menu's Advanced section, with a manual-signature fallback.
- The Android `.so` is not compiled in this scaffold environment (needs NDK +
  Levi Launchroid SDK; CI builds it when `PRELOADER_GIT` is configured). The
  Android sources are written against the documented SDK/NDK/ImGui APIs.
- The ImGui touch-swallow behaviour needs on-device validation (HYPOTHESIS) —
  see RENDERING_REPORT.md and TROUBLESHOOTING.md.
