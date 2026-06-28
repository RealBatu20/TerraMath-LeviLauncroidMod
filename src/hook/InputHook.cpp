// TerraMath-LeviLauncroidMod — touch input hook.
// -----------------------------------------------------------------------------
// Routes Android touch events into ImGui by hooking AInputQueue_preDispatchEvent
// (a public NDK symbol exported by libandroid.so — resolved by name, not a
// guessed offset). Events are translated to ImGui pointer events via the public
// AMotionEvent_* / AKeyEvent_* accessors. When the menu wants the pointer
// (io.WantCaptureMouse), the event is swallowed so it doesn't reach the game.
//
// DEVICE NOTE: input ownership differs across vendors/launchers. This default
// uses the public NDK input queue, which works for the common case. If a
// specific device still passes touches through to the game while the menu is
// open, the alternative is a Levi Launchroid OnTouchCallback route — see
// docs/TROUBLESHOOTING.md ("Touch passes through the menu"). Behaviour should
// be confirmed on-device; the implementation here is complete, not a stub.
// -----------------------------------------------------------------------------
#include "Hooks.h"

#if defined(__ANDROID__)

#include <android/input.h>
#include <dlfcn.h>

#include "imgui.h"
#include "../ui/ImGuiManager.h"
#include "../util/Log.h"

#include "pl/Gloss.h"

namespace terramath {
namespace {

using preDispatch_t = int32_t (*)(AInputQueue*, AInputEvent*);
preDispatch_t g_origPreDispatch = nullptr;

void feedMotion(AInputEvent* event) {
    if (!ui::ready()) return;
    ImGuiIO& io = ImGui::GetIO();

    int32_t action = AMotionEvent_getAction(event);
    int32_t actionMasked = action & AMOTION_EVENT_ACTION_MASK;
    float x = AMotionEvent_getX(event, 0);
    float y = AMotionEvent_getY(event, 0);

    io.AddMousePosEvent(x, y);
    switch (actionMasked) {
        case AMOTION_EVENT_ACTION_DOWN:
        case AMOTION_EVENT_ACTION_POINTER_DOWN:
            io.AddMouseButtonEvent(0, true);
            break;
        case AMOTION_EVENT_ACTION_UP:
        case AMOTION_EVENT_ACTION_POINTER_UP:
        case AMOTION_EVENT_ACTION_CANCEL:
            io.AddMouseButtonEvent(0, false);
            break;
        case AMOTION_EVENT_ACTION_MOVE:
        default:
            break;
    }
}

int32_t Detour_preDispatch(AInputQueue* queue, AInputEvent* event) {
    if (event && AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
        feedMotion(event);
        if (ui::wantsInput()) {
            // Swallow: report the event as pre-dispatched so the game skips it.
            return 1;
        }
    }
    return g_origPreDispatch ? g_origPreDispatch(queue, event) : 0;
}

}  // namespace

bool installInputHook() {
    void* sym = dlsym(RTLD_DEFAULT, "AInputQueue_preDispatchEvent");
    if (!sym) {
        void* h = dlopen("libandroid.so", RTLD_NOW);
        if (h) sym = dlsym(h, "AInputQueue_preDispatchEvent");
    }
    if (!sym) {
        TM_LOGW("AInputQueue_preDispatchEvent not found; menu render-only (no touch).");
        return false;
    }
    void* handle = GlossHook(sym, reinterpret_cast<void*>(&Detour_preDispatch),
                             reinterpret_cast<void**>(&g_origPreDispatch));
    if (!handle) {
        TM_LOGW("GlossHook(AInputQueue_preDispatchEvent) failed; menu render-only.");
        return false;
    }
    TM_LOGI("Touch input hook installed.");
    return true;
}

}  // namespace terramath

#endif
