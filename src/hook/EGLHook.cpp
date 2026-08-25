#include "EGLHook.hpp"
#include "HookManager.hpp"
#include "ElfResolver.hpp"
#include "GUI.hpp"
#include "core/Logger.hpp"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include <dlfcn.h>
#include <vector>
#include <string>
#include <cstdio>
#include <cstring>
#include <thread>
#include <chrono>
#include <atomic>
#include <GLES3/gl3.h>
#include <android/native_window.h>

namespace Hook {

    // ===================== EGL SwapBuffers Hook =====================
    using eglSwapBuffers_t = EGLBoolean (*)(EGLDisplay, EGLSurface);
    static eglSwapBuffers_t Orig_eglSwapBuffers = nullptr;

    using eglSwapBuffersDamage_t = EGLBoolean (*)(EGLDisplay, EGLSurface, EGLint*, EGLint);
    static eglSwapBuffersDamage_t Orig_eglSwapBuffersWithDamageKHR = nullptr;

    static std::atomic<uint64_t> gFrameCounter{0};

    static void DoRender(EGLDisplay dpy, EGLSurface surface) {
        uint64_t frame = gFrameCounter.fetch_add(1) + 1;
        if (frame == 1 || frame == 10 || frame == 100 || frame % 600 == 0) {
            LOGI("[EGLHook] Frame #%llu Active (Display: %p, Surface: %p)", 
                 (unsigned long long)frame, dpy, surface);
        }
        EGLHook::Get().OnSwapBuffers(dpy, surface);
    }

    static EGLBoolean Hooked_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
        DoRender(dpy, surface);
        if (Orig_eglSwapBuffers) return Orig_eglSwapBuffers(dpy, surface);
        return EGL_TRUE;
    }

    static EGLBoolean Hooked_eglSwapBuffersWithDamageKHR(EGLDisplay dpy, EGLSurface surface, EGLint* rects, EGLint n_rects) {
        DoRender(dpy, surface);
        if (Orig_eglSwapBuffersWithDamageKHR) return Orig_eglSwapBuffersWithDamageKHR(dpy, surface, rects, n_rects);
        return EGL_TRUE;
    }

    // ===================== ANativeWindow Post Hook =====================
    using ANativeWindow_unlockAndPost_t = int32_t (*)(ANativeWindow*);
    static ANativeWindow_unlockAndPost_t Orig_ANativeWindow_unlockAndPost = nullptr;

    static int32_t Hooked_ANativeWindow_unlockAndPost(ANativeWindow* window) {
        uint64_t frame = gFrameCounter.fetch_add(1) + 1;
        if (frame == 1 || frame % 600 == 0) {
            LOGI("[NativeWindowHook] Frame #%llu unlockAndPost (Window: %p)", (unsigned long long)frame, window);
        }
        if (Orig_ANativeWindow_unlockAndPost) return Orig_ANativeWindow_unlockAndPost(window);
        return 0;
    }

    // ===================== Vulkan Present Hook =====================
    typedef uint32_t VkResult;
    typedef void* VkQueue;
    struct VkPresentInfoKHR_Minimal {
        uint32_t sType;
        const void* pNext;
    };

    using vkQueuePresentKHR_t = VkResult (*)(VkQueue, const VkPresentInfoKHR_Minimal*);
    static vkQueuePresentKHR_t Orig_vkQueuePresentKHR = nullptr;
    static std::atomic<uint64_t> gVkFrameCounter{0};

    static EGLDisplay gOverlayDpy = EGL_NO_DISPLAY;
    static EGLSurface gOverlaySurf = EGL_NO_SURFACE;
    static EGLContext gOverlayCtx = EGL_NO_CONTEXT;
    static bool gOverlayImGuiInit = false;
    static int gOverlayWidth = 2400;
    static int gOverlayHeight = 1080;

    static VkResult Hooked_vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR_Minimal* pPresentInfo) {
        uint64_t frame = gVkFrameCounter.fetch_add(1) + 1;
        if (frame == 1 || frame == 10 || frame % 600 == 0) {
            LOGI("[VulkanHook] Frame #%llu vkQueuePresentKHR (Queue: %p)", (unsigned long long)frame, queue);
        }

        if (gOverlayCtx != EGL_NO_CONTEXT) {
            EGLContext prevCtx = eglGetCurrentContext();
            EGLDisplay prevDpy = eglGetCurrentDisplay();
            EGLSurface prevRead = eglGetCurrentSurface(EGL_READ);
            EGLSurface prevDraw = eglGetCurrentSurface(EGL_DRAW);

            eglMakeCurrent(gOverlayDpy, gOverlaySurf, gOverlaySurf, gOverlayCtx);

            if (!gOverlayImGuiInit) {
                IMGUI_CHECKVERSION();
                ImGui::CreateContext();
                ImGuiIO& io = ImGui::GetIO();
                io.DisplaySize = ImVec2((float)gOverlayWidth, (float)gOverlayHeight);
                ImGui_ImplOpenGL3_Init("#version 300 es");
                GUI::MainGUI::Get().Initialize();
                gOverlayImGuiInit = true;
                LOGI("[VulkanHook] >>> ImGui Overlay initialized on Vulkan Frame (%dx%d)! <<<", gOverlayWidth, gOverlayHeight);
            }

            ImGuiIO& io = ImGui::GetIO();
            io.DisplaySize = ImVec2((float)gOverlayWidth, (float)gOverlayHeight);
            ImGui_ImplOpenGL3_NewFrame();
            ImGui::NewFrame();
            GUI::MainGUI::Get().Render();
            ImGui::Render();
            glViewport(0, 0, gOverlayWidth, gOverlayHeight);
            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            eglSwapBuffers(gOverlayDpy, gOverlaySurf);

            if (prevCtx != EGL_NO_CONTEXT) {
                eglMakeCurrent(prevDpy, prevDraw, prevRead, prevCtx);
            } else {
                eglMakeCurrent(gOverlayDpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            }
        }

        if (Orig_vkQueuePresentKHR) return Orig_vkQueuePresentKHR(queue, pPresentInfo);
        return 0;
    }

    // ===================== Proc Address Interceptors =====================
    using eglGetProcAddress_t = void* (*)(const char*);
    static eglGetProcAddress_t Orig_eglGetProcAddress = nullptr;

    static void* Hooked_eglGetProcAddress(const char* procname) {
        if (procname) {
            if (strcmp(procname, "eglSwapBuffers") == 0) {
                LOGI("[ProcInterceptor] Intercepted eglGetProcAddress('eglSwapBuffers')");
                return reinterpret_cast<void*>(Hooked_eglSwapBuffers);
            }
            if (strcmp(procname, "eglSwapBuffersWithDamageKHR") == 0 || strcmp(procname, "eglSwapBuffersWithDamageEXT") == 0) {
                LOGI("[ProcInterceptor] Intercepted eglGetProcAddress('%s')", procname);
                return reinterpret_cast<void*>(Hooked_eglSwapBuffersWithDamageKHR);
            }
        }
        return Orig_eglGetProcAddress ? Orig_eglGetProcAddress(procname) : nullptr;
    }

    using vkGetDeviceProcAddr_t = void* (*)(void*, const char*);
    static vkGetDeviceProcAddr_t Orig_vkGetDeviceProcAddr = nullptr;

    static void* Hooked_vkGetDeviceProcAddr(void* device, const char* pName) {
        if (pName && strcmp(pName, "vkQueuePresentKHR") == 0) {
            LOGI("[ProcInterceptor] Intercepted vkGetDeviceProcAddr('vkQueuePresentKHR')");
            void* real = Orig_vkGetDeviceProcAddr ? Orig_vkGetDeviceProcAddr(device, pName) : nullptr;
            if (real && !Orig_vkQueuePresentKHR) {
                Orig_vkQueuePresentKHR = reinterpret_cast<vkQueuePresentKHR_t>(real);
            }
            return reinterpret_cast<void*>(Hooked_vkQueuePresentKHR);
        }
        return Orig_vkGetDeviceProcAddr ? Orig_vkGetDeviceProcAddr(device, pName) : nullptr;
    }

    // ===================== Unique Hook Helper =====================
    static int TryHookUnique(void* addr, void* hookFn, void** origOut, 
                              std::vector<void*>& hookedAddrs, const char* desc) {
        if (!addr) return 0;
        for (void* a : hookedAddrs) {
            if (a == addr) {
                return 0;
            }
        }
        void* orig = nullptr;
        int ret = DobbyHook(addr, hookFn, &orig);
        if (ret == 0) {
            hookedAddrs.push_back(addr);
            if (origOut && !*origOut && orig) *origOut = orig;
            LOGI("[GraphicsHook] SUCCESS: Hooked %s at %p -> trampoline %p", desc, addr, orig);
            return 1;
        } else {
            LOGI("[GraphicsHook] FAILED: Could not hook %s at %p (ret=%d)", desc, addr, ret);
            return 0;
        }
    }

    static void CreateVulkanEGLOverlay() {
        if (gOverlayCtx != EGL_NO_CONTEXT) return;
        gOverlayDpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (gOverlayDpy == EGL_NO_DISPLAY) return;
        EGLint major, minor;
        if (!eglInitialize(gOverlayDpy, &major, &minor)) return;

        EGLint configAttribs[] = {
            EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
            EGL_NONE
        };
        EGLConfig config;
        EGLint numConfigs;
        if (!eglChooseConfig(gOverlayDpy, configAttribs, &config, 1, &numConfigs) || numConfigs == 0) return;

        FILE* wf = fopen("/sys/class/graphics/fb0/virtual_size", "r");
        if (wf) {
            if (fscanf(wf, "%d,%d", &gOverlayWidth, &gOverlayHeight) == 2) {
                LOGI("[VulkanHook] Screen size from fb0: %dx%d", gOverlayWidth, gOverlayHeight);
            }
            fclose(wf);
        }

        EGLint pbufferAttribs[] = { EGL_WIDTH, gOverlayWidth, EGL_HEIGHT, gOverlayHeight, EGL_NONE };
        gOverlaySurf = eglCreatePbufferSurface(gOverlayDpy, config, pbufferAttribs);
        if (gOverlaySurf == EGL_NO_SURFACE) return;

        EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
        gOverlayCtx = eglCreateContext(gOverlayDpy, config, EGL_NO_CONTEXT, contextAttribs);
        if (gOverlayCtx != EGL_NO_CONTEXT) {
            LOGI("[VulkanHook] Independent EGL Overlay context created successfully (%dx%d)", gOverlayWidth, gOverlayHeight);
        }
    }

    bool EGLHook::Initialize() {
        if (bInitialized) return true;

        LOGI("[GraphicsHook] === Starting universal presentation hooks ===");

        std::vector<void*> hookedAddrs;
        int successCount = 0;

        // 1. EGL swap buffers in RTLD_DEFAULT & libEGL.so
        void* defaultSwap = dlsym(RTLD_DEFAULT, "eglSwapBuffers");
        successCount += TryHookUnique(defaultSwap, reinterpret_cast<void*>(Hooked_eglSwapBuffers),
            reinterpret_cast<void**>(&Orig_eglSwapBuffers), hookedAddrs, "RTLD_DEFAULT::eglSwapBuffers");

        void* eglHandle = dlopen("libEGL.so", RTLD_NOW);
        if (eglHandle) {
            void* swap = dlsym(eglHandle, "eglSwapBuffers");
            successCount += TryHookUnique(swap, reinterpret_cast<void*>(Hooked_eglSwapBuffers),
                reinterpret_cast<void**>(&Orig_eglSwapBuffers), hookedAddrs, "libEGL.so::eglSwapBuffers");
            void* swapDmg = dlsym(eglHandle, "eglSwapBuffersWithDamageKHR");
            successCount += TryHookUnique(swapDmg, reinterpret_cast<void*>(Hooked_eglSwapBuffersWithDamageKHR),
                reinterpret_cast<void**>(&Orig_eglSwapBuffersWithDamageKHR), hookedAddrs, "libEGL.so::eglSwapBuffersWithDamageKHR");
        }

        // 2. Hardware Vendor Drivers (Adreno / Mali) via direct safe ELF resolution
        const char* vendorDrivers[] = { "libEGL_adreno.so", "libGLESv2_adreno.so", "libGLESv1_CM_adreno.so", "libmali.so" };
        for (const char* drv : vendorDrivers) {
            void* addr = ElfUtils::ResolveSymbol(drv, "eglSwapBuffers");
            if (addr) {
                char desc[128];
                snprintf(desc, sizeof(desc), "ELF:%s::eglSwapBuffers", drv);
                successCount += TryHookUnique(addr, reinterpret_cast<void*>(Hooked_eglSwapBuffers),
                    reinterpret_cast<void**>(&Orig_eglSwapBuffers), hookedAddrs, desc);
            }
        }

        // 3. Vulkan Hardware Driver vkQueuePresentKHR (Both Loader AND Hardware Driver)
        CreateVulkanEGLOverlay();

        void* vkLoader = dlsym(RTLD_DEFAULT, "vkQueuePresentKHR");
        if (vkLoader) {
            successCount += TryHookUnique(vkLoader, reinterpret_cast<void*>(Hooked_vkQueuePresentKHR),
                reinterpret_cast<void**>(&Orig_vkQueuePresentKHR), hookedAddrs, "libvulkan.so::vkQueuePresentKHR");
        }

        void* vkAdreno = ElfUtils::ResolveSymbol("vulkan.adreno.so", "vkQueuePresentKHR");
        if (vkAdreno) {
            successCount += TryHookUnique(vkAdreno, reinterpret_cast<void*>(Hooked_vkQueuePresentKHR),
                reinterpret_cast<void**>(&Orig_vkQueuePresentKHR), hookedAddrs, "vulkan.adreno.so::vkQueuePresentKHR");
        }

        // 4. ANativeWindow_unlockAndPost (System Display Submission)
        void* nwPost = dlsym(RTLD_DEFAULT, "ANativeWindow_unlockAndPost");
        if (!nwPost) {
            void* androidLib = dlopen("libandroid.so", RTLD_NOW);
            if (androidLib) nwPost = dlsym(androidLib, "ANativeWindow_unlockAndPost");
        }
        if (nwPost) {
            successCount += TryHookUnique(nwPost, reinterpret_cast<void*>(Hooked_ANativeWindow_unlockAndPost),
                reinterpret_cast<void**>(&Orig_ANativeWindow_unlockAndPost), hookedAddrs, "libandroid.so::ANativeWindow_unlockAndPost");
        }

        // 5. Dynamic Proc Address Query Interceptors
        void* gpa = dlsym(RTLD_DEFAULT, "eglGetProcAddress");
        if (gpa) {
            successCount += TryHookUnique(gpa, reinterpret_cast<void*>(Hooked_eglGetProcAddress),
                reinterpret_cast<void**>(&Orig_eglGetProcAddress), hookedAddrs, "eglGetProcAddress");
        }

        void* gda = dlsym(RTLD_DEFAULT, "vkGetDeviceProcAddr");
        if (gda) {
            successCount += TryHookUnique(gda, reinterpret_cast<void*>(Hooked_vkGetDeviceProcAddr),
                reinterpret_cast<void**>(&Orig_vkGetDeviceProcAddr), hookedAddrs, "vkGetDeviceProcAddr");
        }

        LOGI("[GraphicsHook] === All %d presentation hooks installed successfully ===", successCount);

        // 6. Watchdog diagnostics
        std::thread([this]() {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            uint64_t eglFrames = gFrameCounter.load();
            uint64_t vkFrames = gVkFrameCounter.load();
            LOGI("[Watchdog] === 5-Second Frame Count Diagnostic ===");
            LOGI("[Watchdog] Total EGL/NativeWindow Frames: %llu", (unsigned long long)eglFrames);
            LOGI("[Watchdog] Total Vulkan Frames: %llu", (unsigned long long)vkFrames);
            if (eglFrames > 0) LOGI("[Watchdog] >>> Active Presentation Pipeline: EGL/OpenGL ES <<<");
            if (vkFrames > 0) LOGI("[Watchdog] >>> Active Presentation Pipeline: Vulkan <<<");
        }).detach();

        bInitialized = (successCount > 0);
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
                io.DisplaySize = ImVec2((float)ScreenWidth, (float)ScreenHeight);
                ImGui_ImplOpenGL3_Init("#version 300 es");
                GUI::MainGUI::Get().Initialize();
                bImGuiInitialized = true;
                LOGI("[EGLHook] >>> ImGui Overlay Initialized! Screen: %dx%d <<<", ScreenWidth, ScreenHeight);
            }
        }

        if (bImGuiInitialized) {
            GLint last_program, last_array_buffer, last_element_array_buffer, last_vertex_array;
            glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
            glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_array_buffer);
            glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &last_element_array_buffer);
            glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vertex_array);
            GLint last_viewport[4], last_scissor_box[4];
            glGetIntegerv(GL_VIEWPORT, last_viewport);
            glGetIntegerv(GL_SCISSOR_BOX, last_scissor_box);
            GLboolean last_blend = glIsEnabled(GL_BLEND);
            GLboolean last_cull = glIsEnabled(GL_CULL_FACE);
            GLboolean last_depth = glIsEnabled(GL_DEPTH_TEST);
            GLboolean last_scissor = glIsEnabled(GL_SCISSOR_TEST);

            eglQuerySurface(dpy, surface, EGL_WIDTH, &ScreenWidth);
            eglQuerySurface(dpy, surface, EGL_HEIGHT, &ScreenHeight);
            ImGuiIO& io = ImGui::GetIO();
            io.DisplaySize = ImVec2((float)ScreenWidth, (float)ScreenHeight);

            ImGui_ImplOpenGL3_NewFrame();
            ImGui::NewFrame();
            GUI::MainGUI::Get().Render();
            ImGui::Render();
            glViewport(0, 0, ScreenWidth, ScreenHeight);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            glUseProgram(last_program);
            glBindVertexArray(last_vertex_array);
            glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_element_array_buffer);
            glViewport(last_viewport[0], last_viewport[1], last_viewport[2], last_viewport[3]);
            glScissor(last_scissor_box[0], last_scissor_box[1], last_scissor_box[2], last_scissor_box[3]);
            if (last_blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
            if (last_cull) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
            if (last_depth) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
            if (last_scissor) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
        }

        return EGL_TRUE;
    }
}
