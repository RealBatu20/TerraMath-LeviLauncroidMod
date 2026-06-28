#include "ImGuiManager.h"

#if defined(__ANDROID__)

#include <algorithm>
#include <atomic>
#include <chrono>

#include <GLES3/gl3.h>

#include "ConfigUI.h"
#include "imgui.h"
#include "backends/imgui_impl_opengl3.h"
#include "../util/Log.h"

namespace terramath::ui {
namespace {

std::atomic<bool> g_ready{false};
std::atomic<bool> g_wantsInput{false};
std::atomic<bool> g_menuOpen{false};
bool g_glInited = false;
std::chrono::steady_clock::time_point g_lastFrame;

void applyTouchScale(ImGuiIO& io, int width) {
    // Scale fonts/widgets up for high-DPI phone screens so the menu is usable
    // with a finger. Re-applied on resolution change.
    float scale = width > 0 ? std::max(1.0f, width / 1080.0f * 1.6f) : 1.6f;
    io.FontGlobalScale = scale;  // FontGlobalScale drives finger-sized widgets
}

bool ensureInit(int width) {
    if (g_glInited) return true;

    IMGUI_CHECKVERSION();
    if (ImGui::GetCurrentContext() == nullptr) ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;             // don't write imgui.ini on device
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
    ImGui::StyleColorsDark();
    applyTouchScale(io, width);

    if (!ImGui_ImplOpenGL3_Init("#version 300 es")) {
        TM_LOGE("ImGui_ImplOpenGL3_Init failed.");
        return false;
    }
    g_glInited = true;
    g_ready.store(true, std::memory_order_release);
    g_lastFrame = std::chrono::steady_clock::now();
    TM_LOGI("ImGui initialised (GLES3 backend).");
    return true;
}

void drawToggleButton() {
    // Always-visible draggable button that opens/closes the menu. Kept tiny and
    // out of the way; works with a single touch.
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowBgAlpha(0.65f);
    ImGui::SetNextWindowSize(ImVec2(0, 0));
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.02f, io.DisplaySize.y * 0.04f),
                            ImGuiCond_FirstUseEver);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar;
    if (ImGui::Begin("##tm_toggle", nullptr, flags)) {
        if (ImGui::Button(g_menuOpen.load() ? "TerraMath  [x]" : "TerraMath")) {
            g_menuOpen.store(!g_menuOpen.load());
        }
    }
    ImGui::End();
}

}  // namespace

bool ready() { return g_ready.load(std::memory_order_acquire); }
bool wantsInput() { return g_wantsInput.load(std::memory_order_acquire); }

void renderFrame(int width, int height) {
    if (width <= 0 || height <= 0) return;
    if (!ensureInit(width)) return;

    ImGuiIO& io = ImGui::GetIO();
    if (io.DisplaySize.x != (float)width || io.DisplaySize.y != (float)height) {
        io.DisplaySize = ImVec2((float)width, (float)height);
        applyTouchScale(io, width);  // handle rotation / resolution change
    }

    auto now = std::chrono::steady_clock::now();
    float dt = std::chrono::duration<float>(now - g_lastFrame).count();
    g_lastFrame = now;
    io.DeltaTime = dt > 0.0f ? dt : 1.0f / 60.0f;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();

    drawToggleButton();
    if (g_menuOpen.load()) drawConfigWindow();

    ImGui::Render();
    g_wantsInput.store(io.WantCaptureMouse || g_menuOpen.load(), std::memory_order_release);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void invalidate() {
    if (!g_glInited) return;
    ImGui_ImplOpenGL3_Shutdown();
    g_glInited = false;
    g_ready.store(false, std::memory_order_release);
    TM_LOGI("ImGui GL backend invalidated; will re-init next frame.");
}

}  // namespace terramath::ui

#endif
