# Rendering / ImGui Report

## Backend choice
- **Renderer:** ImGui + `imgui_impl_opengl3` compiled for **GLES3**
  (`IMGUI_IMPL_OPENGL_ES3`, shader version `#version 300 es`).
- **Why ImGui:** Bedrock has no editable Create-World screen to mixin into (as
  TerraMath does on Java), so the live formula editor needs a self-contained
  native UI. ImGui over the existing GL context is the standard, fully
  controllable path and avoids touching RenderDragon/bgfx internals.
- **Why the EGL hook (not a bgfx/RenderDragon hook):** `eglSwapBuffers` is a
  public, version-stable choke point right before present. Drawing there is
  renderer-agnostic and survives backend/version changes. (A bgfx-level overlay
  is possible but would require version-specific `libminecraftpe.so` analysis;
  the EGL path needs none.)

## Lifecycle handling (`src/ui/ImGuiManager.cpp`)
- **Lazy init:** ImGui context + GL backend are created on the first frame that
  has a valid surface size, so load order vs. GL readiness doesn't matter.
- **Resolution / orientation changes:** `io.DisplaySize` is refreshed every
  frame from `eglQuerySurface`; font/widget scale is recomputed on change.
- **Context loss / renderer recreation:** `ui::invalidate()` shuts down the GL
  backend; it re-initialises automatically on the next frame.
- **DPI / touch sizing:** `FontGlobalScale` scales the UI up for phone DPI so
  controls are finger-sized; recomputed on resize.
- **No disk writes:** `io.IniFilename = nullptr` (no `imgui.ini` on device).

## Input (`src/hook/InputHook.cpp`)
- Touches arrive via the `AInputQueue_preDispatchEvent` hook and are mapped to
  ImGui pointer events using public `AMotionEvent_*` accessors.
- DOWN/POINTER_DOWN → mouse button down; UP/POINTER_UP/CANCEL → up; MOVE →
  position update.
- When the menu wants the pointer, the event is swallowed from the game.
- **Multi-touch:** pointer index 0 drives the cursor (sufficient for menu
  interaction). Pinch/zoom gestures are not forwarded to the menu.

## Verification status
| Item | Status |
|---|---|
| GLES3 init / shader version | implemented |
| Per-frame display size from EGL | implemented |
| Orientation / resolution change | implemented (recompute on change) |
| Context-loss re-init | implemented (`invalidate()` + lazy re-init) |
| Font/DPI scaling | implemented |
| Touch down/up/move → ImGui | implemented |
| Touch swallow vs game | implemented — **confirm on-device** |
| Crash-safe toggle | implemented (atomic open flag) |

The render and input paths are complete code, not stubs. The items marked
"confirm on-device" depend on per-device input ownership and cannot be asserted
without hardware; see TROUBLESHOOTING.md for the fallback route.

## References consulted (patterns, not copied)
ImGui (`ocornut/imgui`, `imgui_impl_opengl3`), RenderFusion and imGui-Easy for
Android ImGui integration patterns, and the Levi Launchroid input/hook API docs.
No offsets, signatures, or byte patterns were taken from any reference repo.
