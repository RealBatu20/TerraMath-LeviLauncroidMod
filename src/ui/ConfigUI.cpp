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
#include "../terrain/Presets.h"
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
char g_seedBuf[32] = "0";
char g_hintsBuf[512] = {0};
char g_sigBuf[512] = {0};
int  g_presetIdx = 0;
std::string g_status = "Idle.";

std::string configPath() { return g_dataDir + "/terramath.json"; }

void syncBuffersFromCfg() {
    std::snprintf(g_formula, sizeof(g_formula), "%s", g_cfg.terrain.formula.c_str());
    std::snprintf(g_seedBuf, sizeof(g_seedBuf), "%llu",
                  (unsigned long long)g_cfg.terrain.seed);
    std::snprintf(g_hintsBuf, sizeof(g_hintsBuf), "%s", g_cfg.resolver.symbolHints.c_str());
    std::snprintf(g_sigBuf, sizeof(g_sigBuf), "%s", g_cfg.resolver.manualSignature.c_str());
}

void applyEngine(const char* okMsg) {
    g_cfg.terrain.formula = g_formula;
    g_cfg.terrain.seed = std::strtoull(g_seedBuf, nullptr, 10);
    std::string err;
    if (FormulaEngine::instance().apply(g_cfg.terrain, err))
        g_status = g_cfg.terrain.formula.empty() ? "No formula (vanilla terrain)." : okMsg;
    else
        g_status = "Parse error: " + err;
}

void loadOnce() {
    if (g_loaded) return;
    g_cfg = Config::load(configPath());
    syncBuffersFromCfg();
    applyEngine("Active.");
    g_loaded = true;
}

void noiseCombo() {
    static const char* kNames[] = {"None", "Perlin", "Simplex", "Blended", "Normal"};
    int idx = static_cast<int>(g_cfg.terrain.noiseType);
    if (ImGui::Combo("Noise overlay", &idx, kNames, IM_ARRAYSIZE(kNames)))
        g_cfg.terrain.noiseType = static_cast<NoiseType>(idx);
}

void drawPresets() {
    const auto& ps = presets();
    std::vector<const char*> names;
    names.reserve(ps.size() + 1);
    names.push_back("- custom -");
    for (const auto& p : ps) names.push_back(p.name);

    ImGui::SetNextItemWidth(-1);
    if (ImGui::Combo("##presets", &g_presetIdx, names.data(),
                     static_cast<int>(names.size())) &&
        g_presetIdx > 0) {
        applyPreset(ps[g_presetIdx - 1], g_cfg.terrain);
        syncBuffersFromCfg();
        applyEngine("Preset applied. New chunks will use it.");
    }
    ImGui::SameLine();
    if (ImGui::Button("Copy formula")) ImGui::SetClipboardText(g_formula);
}

void drawPreview() {
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

void drawAdvanced() {
    if (!ImGui::CollapsingHeader("Advanced — terrain hook")) return;

    ImGui::TextWrapped(
        "The mod finds Minecraft's terrain generator automatically. You normally "
        "never touch this. Edit only if auto-detect fails on your build.");

    ImGui::Checkbox("Auto-detect generator", &g_cfg.resolver.autoDetect);
    ImGui::InputText("Symbol hints", g_hintsBuf, sizeof(g_hintsBuf));
    {
        static const char* kModes[] = {"height", "density"};
        int modeIdx = (g_cfg.resolver.hookMode == "density") ? 1 : 0;
        if (ImGui::Combo("Hook mode", &modeIdx, kModes, IM_ARRAYSIZE(kModes)))
            g_cfg.resolver.hookMode = kModes[modeIdx];
    }

    if (terrainHookInstalled()) {
        ImGui::TextDisabled("Status: installed (restart to retarget).");
    } else if (ImGui::Button("Detect & install now")) {
        g_cfg.resolver.symbolHints = g_hintsBuf;
        g_cfg.resolver.manualSignature = g_sigBuf;
        std::string s;
        installTerrainHookAuto(g_cfg.resolver, s);
        g_status = s;
    }

    if (ImGui::TreeNode("Manual signature override (optional)")) {
        ImGui::TextWrapped("Leave empty to rely on auto-detect.");
        ImGui::InputText("Signature", g_sigBuf, sizeof(g_sigBuf));
        ImGui::TreePop();
    }
}

}  // namespace

void drawConfigWindow() {
    loadOnce();

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x * 0.6f, io.DisplaySize.y * 0.75f),
                             ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.2f, io.DisplaySize.y * 0.1f),
                            ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("TerraMath", nullptr, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    ImGui::SeparatorText("Presets");
    drawPresets();

    ImGui::SeparatorText("Formula");
    ImGui::TextWrapped("Use x, z (and y) with sin, cos, perlin, clamp, ... e.g. sin(x)*cos(z)*10");
    if (ImGui::InputTextMultiline("##formula", g_formula, sizeof(g_formula),
                                  ImVec2(-1, ImGui::GetTextLineHeight() * 3.0f)))
        g_presetIdx = 0;  // editing => custom
    ImGui::InputText("Seed", g_seedBuf, sizeof(g_seedBuf), ImGuiInputTextFlags_CharsDecimal);

    if (ImGui::Button("Apply")) applyEngine("Applied. New chunks will use this formula.");
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
        g_cfg.terrain.formula = g_formula;
        g_cfg.terrain.seed = std::strtoull(g_seedBuf, nullptr, 10);
        g_cfg.baseFormula = g_formula;
        g_cfg.resolver.symbolHints = g_hintsBuf;
        g_cfg.resolver.manualSignature = g_sigBuf;
        g_status = g_cfg.save(configPath()) ? "Saved." : "Save failed (storage perms?).";
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset")) {
        ResolverConfig keep = g_cfg.resolver;
        g_cfg = Config{};
        g_cfg.resolver = keep;
        g_presetIdx = 0;
        syncBuffersFromCfg();
        applyEngine("Reset to defaults.");
    }

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
    drawAdvanced();

    ImGui::Separator();
    ImGui::TextWrapped("Status: %s", g_status.c_str());
    ImGui::TextDisabled("Terrain hook: %s | Engine: %s",
                        terrainHookInstalled() ? "installed" : "auto/not yet",
                        FormulaEngine::instance().active() ? "active" : "idle");

    ImGui::End();
}

}  // namespace terramath::ui

#endif
