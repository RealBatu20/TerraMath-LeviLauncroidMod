#include "ConfigUI.h"

#if defined(__ANDROID__)

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "imgui.h"
#include "../config/Config.h"
#include "../hook/Hooks.h"
#include "../terrain/FormulaEngine.h"
#include "../terrain/TerrainSettings.h"

namespace terramath::ui {
namespace {

// Slider bounds (addresses passed to ImGui::SliderScalar for ImGuiDataType_Double).
const double kScaleMin = 0.1, kScaleMax = 100.0;
const double kHeightMin = -64.0, kHeightMax = 320.0;
const double kVarMin = 0.0, kVarMax = 256.0;
const double kSmoothMin = 0.0, kSmoothMax = 5.0;
const double kNoiseScaleMin = 1.0, kNoiseScaleMax = 200.0;
const double kNoiseHMin = 0.0, kNoiseHMax = 64.0;

bool g_loaded = false;
Config g_cfg;
char g_formula[1024] = {0};
std::string g_status = "Idle.";
char g_seedBuf[32] = "0";

std::string configPath() { return g_dataDir + "/terramath.json"; }

void loadOnce() {
    if (g_loaded) return;
    g_cfg = Config::load(configPath());
    std::snprintf(g_formula, sizeof(g_formula), "%s", g_cfg.terrain.formula.c_str());
    std::snprintf(g_seedBuf, sizeof(g_seedBuf), "%llu",
                  (unsigned long long)g_cfg.terrain.seed);
    std::string err;
    if (FormulaEngine::instance().apply(g_cfg.terrain, err))
        g_status = g_cfg.terrain.formula.empty() ? "No formula (vanilla terrain)."
                                                 : "Active.";
    else
        g_status = "Parse error: " + err;
    g_loaded = true;
}

void doApply() {
    g_cfg.terrain.formula = g_formula;
    g_cfg.terrain.seed = std::strtoull(g_seedBuf, nullptr, 10);
    std::string err;
    if (FormulaEngine::instance().apply(g_cfg.terrain, err)) {
        g_status = g_cfg.terrain.formula.empty()
                       ? "Applied: empty formula (vanilla terrain)."
                       : "Applied. Newly generated chunks will use this formula.";
    } else {
        g_status = "Parse error: " + err;
    }
}

void noiseCombo() {
    static const char* kNames[] = {"None", "Perlin", "Simplex", "Blended", "Normal"};
    int idx = static_cast<int>(g_cfg.terrain.noiseType);
    if (ImGui::Combo("Noise overlay", &idx, kNames, IM_ARRAYSIZE(kNames)))
        g_cfg.terrain.noiseType = static_cast<NoiseType>(idx);
}

void drawPreview() {
    // Lightweight surface preview: surfaceY across x at z = 0, from the engine's
    // currently-applied settings (press Apply to refresh).
    if (!FormulaEngine::instance().active()) {
        ImGui::TextDisabled("Preview unavailable (no active formula).");
        return;
    }
    constexpr int N = 160;
    static std::vector<float> samples(N);
    float lo = 1e9f, hi = -1e9f;
    for (int i = 0; i < N; ++i) {
        int wx = i - N / 2;
        double y = FormulaEngine::instance().surfaceY(wx, 0);
        float v = (y <= -1e8) ? 0.0f : static_cast<float>(y);
        samples[i] = v;
        lo = std::min(lo, v);
        hi = std::max(hi, v);
    }
    char overlay[64];
    std::snprintf(overlay, sizeof(overlay), "surface y  [%.0f .. %.0f]", lo, hi);
    ImGui::PlotLines("##preview", samples.data(), N, 0, overlay, lo - 1.0f, hi + 1.0f,
                     ImVec2(-1, 120));
}

}  // namespace

void drawConfigWindow() {
    loadOnce();

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x * 0.6f, io.DisplaySize.y * 0.7f),
                             ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.2f, io.DisplaySize.y * 0.12f),
                            ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("TerraMath", nullptr, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    ImGui::TextWrapped(
        "Custom terrain via a math formula. Use x, z (and y) plus functions like "
        "sin, cos, perlin, clamp. Example: sin(x)*cos(z)*10");
    ImGui::Separator();

    ImGui::InputTextMultiline("##formula", g_formula, sizeof(g_formula),
                              ImVec2(-1, ImGui::GetTextLineHeight() * 3.0f));
    ImGui::InputText("Seed", g_seedBuf, sizeof(g_seedBuf),
                     ImGuiInputTextFlags_CharsDecimal);

    if (ImGui::Button("Apply")) doApply();
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
        g_cfg.terrain.formula = g_formula;
        g_cfg.terrain.seed = std::strtoull(g_seedBuf, nullptr, 10);
        g_cfg.baseFormula = g_formula;
        g_status = g_cfg.save(configPath()) ? "Saved." : "Save failed (check storage perms).";
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset")) {
        std::string sig = g_cfg.terrainSignature;  // keep the binary signature
        g_cfg = Config{};
        g_cfg.terrainSignature = sig;
        std::snprintf(g_formula, sizeof(g_formula), "%s", g_cfg.terrain.formula.c_str());
        g_status = "Reset to defaults.";
    }

    ImGui::Separator();
    ImGui::SeparatorText("Terrain parameters");
    ImGui::SliderScalar("Coordinate scale", ImGuiDataType_Double,
                        &g_cfg.terrain.coordinateScale, &kScaleMin, &kScaleMax, "%.2f");
    ImGui::SliderScalar("Base height", ImGuiDataType_Double, &g_cfg.terrain.baseHeight,
                        &kHeightMin, &kHeightMax, "%.1f");
    ImGui::SliderScalar("Height variation", ImGuiDataType_Double,
                        &g_cfg.terrain.heightVariation, &kVarMin, &kVarMax, "%.1f");
    ImGui::SliderScalar("Smoothing factor", ImGuiDataType_Double,
                        &g_cfg.terrain.smoothingFactor, &kSmoothMin, &kSmoothMax, "%.2f");
    ImGui::Checkbox("Density mode", &g_cfg.terrain.useDensityMode);

    ImGui::SeparatorText("Noise");
    noiseCombo();
    if (g_cfg.terrain.noiseType != NoiseType::None) {
        ImGui::SliderScalar("Noise scale X", ImGuiDataType_Double, &g_cfg.terrain.noiseScaleX,
                            &kNoiseScaleMin, &kNoiseScaleMax, "%.1f");
        ImGui::SliderScalar("Noise scale Y", ImGuiDataType_Double, &g_cfg.terrain.noiseScaleY,
                            &kNoiseScaleMin, &kNoiseScaleMax, "%.1f");
        ImGui::SliderScalar("Noise scale Z", ImGuiDataType_Double, &g_cfg.terrain.noiseScaleZ,
                            &kNoiseScaleMin, &kNoiseScaleMax, "%.1f");
        ImGui::SliderScalar("Noise height scale", ImGuiDataType_Double,
                            &g_cfg.terrain.noiseHeightScale, &kNoiseHMin, &kNoiseHMax, "%.2f");
    }

    ImGui::SeparatorText("Preview");
    drawPreview();

    ImGui::Separator();
    ImGui::TextWrapped("Status: %s", g_status.c_str());
    ImGui::TextDisabled("Terrain hook: %s",
                        FormulaEngine::instance().active() ? "engine active" : "inactive");

    ImGui::End();
}

}  // namespace terramath::ui

#endif
