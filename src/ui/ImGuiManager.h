// TerraMath-LeviLauncroidMod — ImGui lifecycle (Android / GLES3).
#pragma once

#if defined(__ANDROID__)

namespace terramath::ui {

// True once the ImGui context + GL backend are created (input hook checks this
// before feeding events, so touches before init are ignored safely).
bool ready();

// Whether the menu currently wants to capture pointer input (used by the input
// hook to decide whether to swallow the touch from the game).
bool wantsInput();

// Called from the eglSwapBuffers hook each frame, with the live surface size.
// Lazily initialises ImGui on first call; safe to call every frame.
void renderFrame(int width, int height);

// Tear down the GL backend + context (called when the GL context is lost, e.g.
// app background / renderer recreation). Re-inits automatically next frame.
void invalidate();

}  // namespace terramath::ui

#endif
