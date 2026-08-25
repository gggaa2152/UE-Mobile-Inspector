#pragma once

#include <EGL/egl.h>
#include <GLES3/gl3.h>

namespace Hook {

    class EGLHook {
    public:
        static EGLHook& Get() {
            static EGLHook instance;
            return instance;
        }

        bool Initialize();
        bool IsInitialized() const { return bInitialized; }
        void Shutdown();

        // Hooked eglSwapBuffers callback
        EGLBoolean OnSwapBuffers(EGLDisplay dpy, EGLSurface surface);

    private:
        EGLHook() = default;
        bool bInitialized = false;
        bool bImGuiInitialized = false;

        int ScreenWidth = 0;
        int ScreenHeight = 0;
    };
}
