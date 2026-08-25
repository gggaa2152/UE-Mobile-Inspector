#include "TouchHook.hpp"
#include "imgui.h"
#include "imgui_impl_android.h"

namespace Hook {

    bool TouchHook::Initialize() {
        bInitialized = true;
        return true;
    }

    bool TouchHook::HandleInputEvent(AInputEvent* event) {
        if (!event) return false;

        int32_t type = AInputEvent_getType(event);
        if (type == AINPUT_EVENT_TYPE_MOTION) {
            int32_t action = AMotionEvent_getAction(event);
            float x = AMotionEvent_getX(event, 0);
            float y = AMotionEvent_getY(event, 0);

            ImGuiIO& io = ImGui::GetIO();
            io.MousePos = ImVec2(x, y);

            if ((action & AMOTION_EVENT_ACTION_MASK) == AMOTION_EVENT_ACTION_DOWN) {
                io.MouseDown[0] = true;
            } else if ((action & AMOTION_EVENT_ACTION_MASK) == AMOTION_EVENT_ACTION_UP ||
                       (action & AMOTION_EVENT_ACTION_MASK) == AMOTION_EVENT_ACTION_CANCEL) {
                io.MouseDown[0] = false;
            }

            // Return true to consume event if ImGui is capturing touch
            return io.WantCaptureMouse;
        }

        return false;
    }
}
