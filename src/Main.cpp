// TerraMath-LeviLauncroidMod — native mod entry point.
// -----------------------------------------------------------------------------
// Loaded by Levi Launchroid into the Minecraft Bedrock process. The constructor
// runs at library load: it initialises Gloss, resolves a writable data dir,
// loads config, applies the formula to the engine, then installs the UI render
// hook and (if a signature is configured) the terrain-generation hook.
// -----------------------------------------------------------------------------
#include "hook/Hooks.h"

#if defined(__ANDROID__)

#include <sys/stat.h>
#include <unistd.h>

#include <string>

#include "config/Config.h"
#include "terrain/FormulaEngine.h"
#include "util/Log.h"

#include "pl/Gloss.h"

namespace terramath {

std::string g_dataDir;

namespace {

bool ensureDir(const std::string& path) {
    if (path.empty()) return false;
    std::string acc;
    for (size_t i = 0; i < path.size(); ++i) {
        acc += path[i];
        if (path[i] == '/' || i + 1 == path.size()) {
            if (acc != "/") mkdir(acc.c_str(), 0775);
        }
    }
    return access(path.c_str(), W_OK) == 0;
}

std::string resolveDataDir() {
    const char* candidates[] = {
        "/sdcard/games/com.mojang/terramath",
        "/storage/emulated/0/games/com.mojang/terramath",
        "/data/data/com.mojang.minecraftpe/files/terramath",
    };
    for (const char* c : candidates) {
        if (ensureDir(c)) return c;
    }
    return "/data/local/tmp/terramath";  // last-resort
}

}  // namespace

}  // namespace terramath

__attribute__((constructor)) static void TerraMath_Init() {
    using namespace terramath;

    GlossInit(true);

    g_dataDir = resolveDataDir();
    ensureDir(g_dataDir);
    TM_LOGI("TerraMath data dir: %s", g_dataDir.c_str());

    const std::string cfgPath = g_dataDir + "/terramath.json";
    Config cfg = Config::load(cfgPath);

    // Write a default config on first run so users have a file to edit.
    if (access(cfgPath.c_str(), F_OK) != 0) {
        cfg.save(cfgPath);
        TM_LOGI("Wrote default config to %s", cfgPath.c_str());
    }

    std::string err;
    if (FormulaEngine::instance().apply(cfg.terrain, err)) {
        TM_LOGI("Formula engine ready (%s).",
                cfg.terrain.formula.empty() ? "no formula" : cfg.terrain.formula.c_str());
    } else {
        TM_LOGW("Formula parse failed: %s", err.c_str());
    }

    installUiHooks();
    installTerrainHook(cfg.terrainSignature);

    TM_LOGI("TerraMath-LeviLauncroidMod initialised.");
}

#endif
