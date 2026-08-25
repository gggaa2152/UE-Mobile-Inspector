#include "EGLHook.hpp"
#include "dobby.h"
#include "GUI.hpp"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include <dlfcn.h>
#include <android/log.h>

#define LOG_TAG "UE-Mobile-Inspector"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

namespace Hook {

    using eglSwapBuffers_t = EGLBoolean (*)(EGLDisplay, EGLSurface);
    static eglSwapBuffers_t Orig_eglSwapBuffers = nullptr;

    static EGLBoolean Hooked_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
        EGLHook::Get().OnSwapBuffers(dpy, surface);
        return Orig_eglSwapBuffers ? Orig_eglSwapBuffers(dpy, surface) : EGL_TRUE;
    }

    bool EGLHook::Initialize() {
        if (bInitialized) return true;

        void* eglHandle = dlopen("libEGL.so", RTLD_NOW);
        if (!eglHandle) {
            LOGI("Failed to open libEGL.so");
            return false;
        }

        void* swapAddr = dlsym(eglHandle, "eglSwapBuffers");
        if (!swapAddr) {
            LOGI("Failed to find eglSwapBuffers");
            return false;
        }

        int ret = DobbyHook(swapAddr, reinterpret_cast<void*>(Hooked_eglSwapBuffers), 
                            reinterpret_cast<void**>(&Orig_eglSwapBuffers));

        bInitialized = (ret == 0);
        LOGI("eglSwapBuffers Hook Status: %s", bInitialized ? "Success" : "Failed");
        return bInitialized;
    }

    void EGLHook::Shutdown() {
        if (bImGuiInitialized) {
            ImGui_ImplOpenGL3_Shutdown();
            ImGui::DestroyContext();
            bImGuiInitialized = false;
        }
    }

    EGLBoolean EGLHook::OnSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
        if (!bImGuiInitialized) {
            eglQuerySurface(dpy, surface, EGL_WIDTH, &ScreenWidth);
            eglQuerySurface(dpy, surface, EGL_HEIGHT, &ScreenHeight);

            if (ScreenWidth > 0 && ScreenHeight > 0) {
                IMGUI_CHECKVERSION();
                ImGui::CreateContext();

                ImGuiIO& io = ImGui::GetIO();
                io.DisplaySize = ImVec2(static_cast<float>(ScreenWidth), static_cast<float>(ScreenHeight));

                ImGui_ImplOpenGL3_Init("#version 300 es");
                GUI::MainGUI::Get().Initialize();

                bImGuiInitialized = true;
                LOGI("ImGui initialized with GLES3 backend (%dx%d)", ScreenWidth, ScreenHeight);
            }
        }

        if (bImGuiInitialized) {
            ImGui_ImplOpenGL3_NewFrame();
            
            ImGuiIO& io = ImGui::GetIO();
            eglQuerySurface(dpy, surface, EGL_WIDTH, &ScreenWidth);
            eglQuerySurface(dpy, surface, EGL_HEIGHT, &ScreenHeight);
            io.DisplaySize = ImVec2(static_cast<float>(ScreenWidth), static_cast<float>(ScreenHeight));

            ImGui::NewFrame();

            // Render Our Tool UI
            GUI::MainGUI::Get().Render();

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        }

        return EGL_TRUE;
    }
}
