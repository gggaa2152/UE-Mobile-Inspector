#pragma once

#include <android/input.h>

namespace Hook {

    class TouchHook {
    public:
        static TouchHook& Get() {
            static TouchHook instance;
            return instance;
        }

        bool Initialize();
        bool HandleInputEvent(AInputEvent* event);

    private:
        TouchHook() = default;
        bool bInitialized = false;
    };
}
