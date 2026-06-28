// TerraMath-LeviLauncroidMod — render hook.
// -----------------------------------------------------------------------------
// Draws the ImGui menu by hooking eglSwapBuffers (a public, exported EGL symbol
// from libEGL.so — resolved by name via dlsym, NOT a guessed libminecraftpe
// offset). Every frame, just before the game presents, we query the live
// surface size, render the overlay into the current GL context, then call the
// original swap. This is renderer-agnostic and survives context/resolution
// changes (ImGuiManager re-inits on demand).
// -----------------------------------------------------------------------------
#include "Hooks.h"

#if defined(__ANDROID__)

#include <EGL/egl.h>
#include <dlfcn.h>

#include "../ui/ImGuiManager.h"
#include "../util/Log.h"

#include "pl/Gloss.h"

namespace terramath {
namespace {

using eglSwapBuffers_t = EGLBoolean (*)(EGLDisplay, EGLSurface);
eglSwapBuffers_t g_origSwap = nullptr;

EGLBoolean Detour_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    EGLint w = 0, h = 0;
    if (surface != EGL_NO_SURFACE) {
        eglQuerySurface(dpy, surface, EGL_WIDTH, &w);
        eglQuerySurface(dpy, surface, EGL_HEIGHT, &h);
    }
    if (w > 0 && h > 0) {
        ui::renderFrame(w, h);
    }
    return g_origSwap ? g_origSwap(dpy, surface) : EGL_FALSE;
}

}  // namespace

bool installUiHooks() {
    void* eglHandle = dlopen("libEGL.so", RTLD_NOW | RTLD_NOLOAD);
    if (!eglHandle) eglHandle = dlopen("libEGL.so", RTLD_NOW);
    void* sym = eglHandle ? dlsym(eglHandle, "eglSwapBuffers")
                          : dlsym(RTLD_DEFAULT, "eglSwapBuffers");
    if (!sym) {
        TM_LOGE("Could not resolve eglSwapBuffers; UI hook not installed.");
        return false;
    }

    void* handle = GlossHook(sym, reinterpret_cast<void*>(&Detour_eglSwapBuffers),
                             reinterpret_cast<void**>(&g_origSwap));
    if (!handle) {
        TM_LOGE("GlossHook(eglSwapBuffers) failed.");
        return false;
    }

    extern bool installInputHook();  // defined in InputHook.cpp
    bool input = installInputHook();
    TM_LOGI("UI render hook installed (input hook: %s).", input ? "ok" : "fallback");
    return true;
}

}  // namespace terramath

#endif
