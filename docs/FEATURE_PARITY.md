# Feature Parity Report

Legend: ✅ full parity · 🟡 partial / disclosed difference · 🔵 needs binary or
device validation · ⬜ intentionally omitted.

| TerraMath feature | Status | Notes |
|---|---|---|
| Formula input (`x`, `y`, `z`) | ✅ | Parsed + evaluated natively |
| 50+ math functions (trig, hyperbolic, inverse, roots, log/exp, gamma, erf, beta, gcd/lcm, clamp, sigmoid, …) | ✅ | Ported from `MathExtensions` + registry; unit-tested |
| Constants (π/pi, e, φ/phi, ζ3, catalan/K, α, δ, Ω + aliases) | ✅ | Identical values; Greek (UTF-8) and ASCII aliases |
| `^` power operator | ✅ | Right-associative, negative exponents |
| `!` factorial (postfix) | ✅ | Generalised via `gamma(n+1)` |
| Implicit equation mode (`lhs = rhs`) | ✅ | Rewritten to `(rhs)-(lhs)` |
| Coordinate scale | ✅ | |
| Base height | ✅ | |
| Height variation | ✅ | |
| Smoothing factor (spatial averaging) | ✅ | 5-point (height) / 7-point (equation), matches Java |
| Density mode | ✅ | Same formula branches |
| Noise overlay (Perlin/Simplex/Blended/Normal) | 🟡 | Implemented + seeded, but **not bit-identical** to MC Java noise (CONVERSION_REPORT §D) |
| Random functions (`rand`, `randnormal`, `randrange`) | 🟡 | Deterministic seeded RNG instead of `ThreadLocalRandom` |
| Per-world formula persistence | 🟡 | Local JSON config instead of world-NBT `SavedData` (NBT_ANALYSIS.md) |
| Config file (`terramath.json`) | ✅ | Port of `ModConfig`; round-trip tested |
| Settings UI | 🟡 | ImGui menu (formula editor, sliders, noise, status) — not a 1:1 of the Java Create-World tab |
| 3D real-time preview | 🟡 | Replaced by a lightweight 1D surface-height preview (`PlotLines`) |
| Multi-language UI strings | ⬜ | English-only menu; original lang files are not reused |
| Server config (`config/terramath.json`) | ⬜ | Single-player/client-side scope; no server sync layer |
| Actual world terrain override | 🔵 | Hook target **auto-detected** at runtime (demangled symbol scan); confirm on your Bedrock build, Advanced section retargets if needed (SIGNATURE_ANALYSIS.md) |
| One-tap formula presets | ✅ | 12 curated presets with matching params (not in the Java mod — a UX addition) |
| Copy formula to clipboard | ✅ | Menu "Copy formula" button |
| Touch interaction with the menu | 🔵 | Implemented via `AInputQueue` hook; confirm swallow behaviour on-device |

## Closest-compliant choices where exact parity isn't feasible
- **Noise fidelity:** reproducing MC Java's internal noise bit-for-bit would
  require porting version-specific Java RNG/noise classes; we provide standard
  coherent noise and disclose the difference rather than fake parity.
- **UI:** Bedrock has no editable Create-World tab to mixin into; ImGui is the
  practical, fully-controllable native UI for live formula editing.
- **Preview:** a true 3D preview needs its own render target; a 1D/column
  surface plot conveys the formula shape cheaply and reliably.

## Recommended future work
- Find and wire the Bedrock world-seed source so noise/`rand` track the actual
  world seed automatically (currently a UI/config field).
- Optional: write the active formula into the world's level data so a generated
  world re-loads with the same terrain settings (NBT_ANALYSIS.md).
- Optional: a 2D top-down heightmap preview panel.
