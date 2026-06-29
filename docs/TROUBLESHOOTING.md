# Troubleshooting

Logs: `adb logcat -s TerraMath`. Every major step logs (data dir, engine
status, each hook).

## The menu doesn't appear
- Confirm the `.so` actually loaded: look for `TerraMath-LeviLauncroidMod
  initialised.` in logcat. If absent, the loader didn't inject it — re-check the
  mods directory and Levi Launchroid setup.
- Confirm the render hook: `UI render hook installed`. If you see `Could not
  resolve eglSwapBuffers`, the device's EGL library name/path differs; check
  that `libEGL.so` is present.
- The toggle button is top-left at ~2% x / 4% y. If off-screen after a rotation,
  it repositions on `FirstUseEver`; clear nothing — just rotate back once.

## Touch passes through the menu (taps reach the game)
This is the known per-device input-ownership case.
- First confirm the input hook installed: `Touch input hook installed.` If you
  see the fallback warning instead, `AInputQueue_preDispatchEvent` wasn't
  resolvable on this device.
- If installed but touches still leak through, the game consumes input on a path
  that doesn't pass through `AInputQueue`. Alternative route: use a Levi
  Launchroid **OnTouchCallback** / input-callback hook to feed ImGui and gate
  the game's handler (see <https://liteldev.github.io/LeviLaunchroid/api/input>).
  Forward events to ImGui exactly as `InputHook.cpp` does and return "consumed"
  when `ui::wantsInput()`.

## Menu renders but is tiny / huge
- Scaling derives from surface width (`FontGlobalScale`). On unusual aspect
  ratios adjust the factor in `ImGuiManager.cpp::applyTouchScale`.

## Terrain doesn't change after Apply
- Apply affects **newly generated** chunks only — fly to ungenerated terrain or
  make a new world.
- Check the terrain hook installed: logcat should show `Auto-resolver: matched
  '…' at 0x…` and `Terrain hook auto-installed`. If instead you see `Auto-resolver:
  no symbol matched the hints`, this Bedrock build is stripped of the generator
  symbols — open the menu's **Advanced** section, adjust **Symbol hints** (or set
  a manual signature, SIGNATURE_ANALYSIS.md), and tap **Detect & install now**.
- The engine being "active" only means the formula compiled — the world hook is
  separate (shown as *Terrain hook: installed* at the bottom of the menu).

## "Parse error" in the menu
- The status line shows the parser message (e.g. unknown function, wrong arity,
  unmatched paren). Function names are case-insensitive; variables are `x y z`;
  constants include `pi/π, e, phi/φ, …`. `^` is power, postfix `!` is factorial.

## Crash on world load after enabling the terrain hook
- Almost always a **signature/prototype mismatch**: the resolved function isn't
  the one assumed, or its argument order differs. Re-verify in IDA that the hit
  is the generation function and that its prototype matches the seam you chose
  (column-height vs density — HOOK_ANALYSIS.md). Clear `terrainSignature` to
  recover, then re-derive.

## Config changes don't persist
- Press **Save** in the menu (Apply alone doesn't write the file).
- Check the data dir is writable; logcat prints the resolved path. If it fell
  back to `/data/local/tmp/terramath`, storage permissions blocked the normal
  locations.

## After a Minecraft update, terrain override stopped working
- Signatures are **version-specific**. Re-derive `terrainSignature` against the
  new `libminecraftpe.so` (SIGNATURE_ANALYSIS.md). The engine/menu keep working
  regardless.
