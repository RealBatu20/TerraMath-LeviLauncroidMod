# Hook Analysis

Three hooks. Two are resolved by **public exported symbol name** (verifiable,
not binary-version-specific). One — the terrain hook — targets
`libminecraftpe.so` and needs a signature from your binary.

## 1. Render hook — `eglSwapBuffers` (libEGL.so)  ✅ verifiable
- **File:** `src/hook/EglHook.cpp`
- **Target:** `eglSwapBuffers(EGLDisplay, EGLSurface)` — a public EGL entry
  point exported by `libEGL.so`. Resolved with `dlsym`, hooked with `GlossHook`.
- **Why here:** it's the single choke-point every frame just before present,
  independent of Minecraft's renderer internals — so the overlay survives
  RenderDragon/bgfx specifics, resolution changes and context recreation.
- **Detour:** query live surface size via `eglQuerySurface`, call
  `ui::renderFrame(w, h)` (lazy ImGui init + draw), then the original swap.
- **Risk:** low. No `libminecraftpe.so` offsets involved. If multiple surfaces
  exist, rendering keys off the surface actually being presented.

## 2. Input hook — `AInputQueue_preDispatchEvent` (libandroid.so)  ✅ verifiable / 🔵 device
- **File:** `src/hook/InputHook.cpp`
- **Target:** `AInputQueue_preDispatchEvent(AInputQueue*, AInputEvent*)` — a
  public NDK symbol. Resolved via `dlsym`, hooked with `GlossHook`.
- **Detour:** translate motion events to ImGui pointer events via public
  `AMotionEvent_*` accessors; when `io.WantCaptureMouse` (menu has the pointer),
  return `1` to swallow the event from the game.
- **Risk:** the *swallow* semantics and input ownership vary by device/launcher.
  The implementation is complete; confirm on-device. Fallback route documented
  in TROUBLESHOOTING.md ("Touch passes through the menu").

## 3. Terrain hook — `libminecraftpe.so`  🔵 needs your binary
- **File:** `src/hook/TerrainHook.cpp`
- **Target:** Bedrock's native column/terrain generation function. Resolved by
  **signature** (`pl::signature::pl_resolve_signature(sig, "libminecraftpe.so")`)
  read from config (`terrainSignature`). Hooked with `GlossHook`.
- **Integration seam (`TerrainColumnHeightFn`):** the detour assumes the
  resolved function has the shape
  ```cpp
  int generate(void* self, int worldX, int worldZ);   // returns surface height
  ```
  The detour calls the original (so biomes/decoration still run), then replaces
  the height with `FormulaEngine::surfaceY(worldX, worldZ)` clamped to
  `[-64, 319]`.
- **If your generator isn't column-height shaped:** switch to the density path.
  `FormulaEngine::compute(x, y, z)` returns the Java-equivalent density sign
  (`+1` solid / `-1` air). Hook whatever per-voxel density/solidity function you
  identify and return solid when `compute(...) >= 0`. Both paths are implemented;
  only the function prototype + signature change.
- **Why no hardcoded signature:** offsets/bytes differ per Bedrock build and
  ABI. A guessed pattern would be fabricated and crash-prone. Empty signature →
  hook disabled (engine/UI still run). See SIGNATURE_ANALYSIS.md.

## Hook safety summary
| Hook | Resolution | Crash risk without binary | Reversible |
|---|---|---|---|
| eglSwapBuffers | symbol (dlsym) | none | yes (Gloss unhook) |
| AInputQueue_preDispatchEvent | symbol (dlsym) | none | yes |
| terrain gen | signature (config) | n/a — disabled if empty | yes |

All three call the original function and add behaviour around it (no
destructive patching), keeping the game stable and the changes reversible.
