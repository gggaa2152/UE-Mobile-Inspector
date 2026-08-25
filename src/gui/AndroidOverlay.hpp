#pragma once

#include <jni.h>

namespace GUI {
    class AndroidOverlay {
    public:
        static AndroidOverlay& Get() {
            static AndroidOverlay instance;
            return instance;
        }

        bool Initialize(JavaVM* vm = nullptr);
        void Show();
        void Hide();
        void Toggle();

    private:
        AndroidOverlay() = default;
        bool bInitialized = false;
        JavaVM* gJavaVM = nullptr;
    };
}
