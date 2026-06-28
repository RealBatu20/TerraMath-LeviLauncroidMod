// TerraMath-LeviLauncroidMod — the formula/terrain settings window.
#pragma once

#if defined(__ANDROID__)

namespace terramath::ui {

// Draws the TerraMath window contents (formula editor, sliders, noise,
// Apply/Reset/Save). Call between ImGui::NewFrame and ImGui::Render.
void drawConfigWindow();

}  // namespace terramath::ui

#endif
