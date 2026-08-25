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
        if (frame == 1 || frame == 10 || frame == 100 || frame % 1000 == 0) {
            LOGI("[EGLHook] Frame #%llu SwapBuffers called (dpy=%p, surf=%p)", 
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

    // ===================== Vulkan Present Hook =====================
    // Minimal Vulkan types (no Vulkan headers needed)
    typedef uint32_t VkResult;
    typedef void* VkQueue;
    struct VkPresentInfoKHR_Minimal {
        uint32_t sType;
        const void* pNext;
        // ... rest doesn't matter for our hook
    };

    using vkQueuePresentKHR_t = VkResult (*)(VkQueue, const VkPresentInfoKHR_Minimal*);
    static vkQueuePresentKHR_t Orig_vkQueuePresentKHR = nullptr;
    static std::atomic<uint64_t> gVkFrameCounter{0};
    static std::atomic<bool> gVulkanDetected{false};

    // Independent EGL overlay state for Vulkan games
    static EGLDisplay gOverlayDpy = EGL_NO_DISPLAY;
    static EGLSurface gOverlaySurf = EGL_NO_SURFACE;
    static EGLContext gOverlayCtx = EGL_NO_CONTEXT;
    static bool gOverlayImGuiInit = false;
    static int gOverlayWidth = 0;
    static int gOverlayHeight = 0;

    static VkResult Hooked_vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR_Minimal* pPresentInfo) {
        uint64_t frame = gVkFrameCounter.fetch_add(1) + 1;
        if (frame == 1) {
            gVulkanDetected.store(true);
            LOGI("[VulkanHook] >>> Vulkan rendering CONFIRMED! Frame #1 vkQueuePresentKHR called <<<");
        }
        if (frame == 10 || frame == 100 || frame % 1000 == 0) {
            LOGI("[VulkanHook] Frame #%llu vkQueuePresentKHR (queue=%p)", (unsigned long long)frame, queue);
        }

        // Render overlay using independent EGL context
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
                LOGI("[VulkanHook] ImGui overlay initialized on EGL PBuffer (%dx%d)", gOverlayWidth, gOverlayHeight);
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

            // Restore previous context
            if (prevCtx != EGL_NO_CONTEXT) {
                eglMakeCurrent(prevDpy, prevDraw, prevRead, prevCtx);
            } else {
                eglMakeCurrent(gOverlayDpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            }
        }

        return Orig_vkQueuePresentKHR(queue, pPresentInfo);
    }

    // ===================== Unique Hook Helper =====================
    static int TryHookUnique(void* addr, void* hookFn, void** origOut, 
                              std::vector<void*>& hookedAddrs, const char* desc) {
        if (!addr) return 0;
        for (void* a : hookedAddrs) {
            if (a == addr) {
                LOGI("[EGLHook] Skipping duplicate address %p (%s)", addr, desc);
                return 0;
            }
        }
        void* orig = nullptr;
        int ret = DobbyHook(addr, hookFn, &orig);
        if (ret == 0) {
            hookedAddrs.push_back(addr);
            if (origOut && !*origOut && orig) *origOut = orig;
            LOGI("[EGLHook] SUCCESS: Hooked %s at %p -> trampoline %p", desc, addr, orig);
            return 1;
        } else {
            LOGI("[EGLHook] FAILED: Could not hook %s at %p (ret=%d)", desc, addr, ret);
            return 0;
        }
    }

    // ===================== EGL Overlay for Vulkan =====================
    static void CreateVulkanEGLOverlay() {
        LOGI("[VulkanHook] Creating independent EGL overlay for Vulkan game...");
        
        gOverlayDpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (gOverlayDpy == EGL_NO_DISPLAY) {
            LOGI("[VulkanHook] FAILED: eglGetDisplay returned EGL_NO_DISPLAY");
            return;
        }
        
        EGLint major, minor;
        if (!eglInitialize(gOverlayDpy, &major, &minor)) {
            LOGI("[VulkanHook] FAILED: eglInitialize failed");
            return;
        }
        LOGI("[VulkanHook] EGL %d.%d initialized", major, minor);

        EGLint configAttribs[] = {
            EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_ALPHA_SIZE, 8,
            EGL_NONE
        };
        EGLConfig config;
        EGLint numConfigs;
        if (!eglChooseConfig(gOverlayDpy, configAttribs, &config, 1, &numConfigs) || numConfigs == 0) {
            LOGI("[VulkanHook] FAILED: eglChooseConfig failed");
            return;
        }

        // Try to get screen dimensions
        gOverlayWidth = 2400;
        gOverlayHeight = 1080;
        
        // Read actual screen size from /sys
        FILE* wf = fopen("/sys/class/graphics/fb0/virtual_size", "r");
        if (wf) {
            if (fscanf(wf, "%d,%d", &gOverlayWidth, &gOverlayHeight) == 2) {
                LOGI("[VulkanHook] Screen size from fb0: %dx%d", gOverlayWidth, gOverlayHeight);
            }
            fclose(wf);
        }

        EGLint pbufferAttribs[] = {
            EGL_WIDTH, gOverlayWidth,
            EGL_HEIGHT, gOverlayHeight,
            EGL_NONE
        };
        gOverlaySurf = eglCreatePbufferSurface(gOverlayDpy, config, pbufferAttribs);
        if (gOverlaySurf == EGL_NO_SURFACE) {
            LOGI("[VulkanHook] FAILED: eglCreatePbufferSurface failed (err=0x%x)", eglGetError());
            return;
        }

        EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
        gOverlayCtx = eglCreateContext(gOverlayDpy, config, EGL_NO_CONTEXT, contextAttribs);
        if (gOverlayCtx == EGL_NO_CONTEXT) {
            LOGI("[VulkanHook] FAILED: eglCreateContext failed (err=0x%x)", eglGetError());
            return;
        }

        LOGI("[VulkanHook] EGL overlay context created successfully (PBuffer %dx%d)", gOverlayWidth, gOverlayHeight);
    }

    // ===================== Main Initialize =====================
    bool EGLHook::Initialize() {
        if (bInitialized) return true;

        LOGI("[EGLHook] === Starting comprehensive graphics hook initialization ===");

        std::vector<void*> hookedAddrs;
        int successCount = 0;

        // ======== 1. Standard dlsym hooks ========
        void* defaultSwap = dlsym(RTLD_DEFAULT, "eglSwapBuffers");
        LOGI("[EGLHook] dlsym(RTLD_DEFAULT, eglSwapBuffers) = %p", defaultSwap);
        successCount += TryHookUnique(defaultSwap, 
            reinterpret_cast<void*>(Hooked_eglSwapBuffers),
            reinterpret_cast<void**>(&Orig_eglSwapBuffers), hookedAddrs, "RTLD_DEFAULT::eglSwapBuffers");

        void* eglHandle = dlopen("libEGL.so", RTLD_NOW);
        if (eglHandle) {
            void* swap = dlsym(eglHandle, "eglSwapBuffers");
            LOGI("[EGLHook] dlsym(libEGL.so, eglSwapBuffers) = %p", swap);
            successCount += TryHookUnique(swap,
                reinterpret_cast<void*>(Hooked_eglSwapBuffers),
                reinterpret_cast<void**>(&Orig_eglSwapBuffers), hookedAddrs, "libEGL.so::eglSwapBuffers");

            void* swapDmg = dlsym(eglHandle, "eglSwapBuffersWithDamageKHR");
            successCount += TryHookUnique(swapDmg,
                reinterpret_cast<void*>(Hooked_eglSwapBuffersWithDamageKHR),
                reinterpret_cast<void**>(&Orig_eglSwapBuffersWithDamageKHR), hookedAddrs, "libEGL.so::eglSwapBuffersWithDamageKHR");
        }

        // ======== 2. Manual ELF resolution for vendor drivers (bypass namespace) ========
        const char* vendorDrivers[] = {
            "libEGL_adreno.so",
            "libGLESv2_adreno.so",
            "libGLESv1_CM_adreno.so",
            "libmali.so",
            "libGLES_mali.so",
            "egl.cfg",
        };
        const char* vendorSymbols[] = {
            "eglSwapBuffers",
            "eglSwapBuffersWithDamageKHR",
            "eglSwapBuffersWithDamageEXT",
        };

        LOGI("[EGLHook] === Manual ELF resolution for vendor GPU drivers ===");
        for (const char* drv : vendorDrivers) {
            uintptr_t drvBase = ElfUtils::FindModuleBase(drv);
            if (!drvBase) continue;
            LOGI("[EGLHook] Found vendor driver %s at base 0x%lx", drv, (unsigned long)drvBase);

            for (const char* sym : vendorSymbols) {
                void* addr = ElfUtils::ResolveSymbol(drv, sym);
                if (addr) {
                    char desc[256];
                    snprintf(desc, sizeof(desc), "ELF:%s::%s", drv, sym);
                    
                    bool isSwapDamage = (strstr(sym, "Damage") != nullptr);
                    if (isSwapDamage) {
                        successCount += TryHookUnique(addr,
                            reinterpret_cast<void*>(Hooked_eglSwapBuffersWithDamageKHR),
                            reinterpret_cast<void**>(&Orig_eglSwapBuffersWithDamageKHR), hookedAddrs, desc);
                    } else {
                        successCount += TryHookUnique(addr,
                            reinterpret_cast<void*>(Hooked_eglSwapBuffers),
                            reinterpret_cast<void**>(&Orig_eglSwapBuffers), hookedAddrs, desc);
                    }
                }
            }
        }

        // ======== 3. Vulkan vkQueuePresentKHR hook ========
        LOGI("[EGLHook] === Vulkan detection ===");
        void* vkPresent = nullptr;

        // Try dlsym first
        void* vkHandle = dlopen("libvulkan.so", RTLD_NOW);
        if (vkHandle) {
            vkPresent = dlsym(vkHandle, "vkQueuePresentKHR");
            LOGI("[EGLHook] dlsym(libvulkan.so, vkQueuePresentKHR) = %p", vkPresent);
        }
        // Fallback: manual ELF
        if (!vkPresent) {
            vkPresent = ElfUtils::ResolveSymbol("libvulkan.so", "vkQueuePresentKHR");
            LOGI("[EGLHook] ELF resolve(libvulkan.so, vkQueuePresentKHR) = %p", vkPresent);
        }
        // Try vendor Vulkan
        if (!vkPresent) {
            vkPresent = ElfUtils::ResolveSymbol("vulkan.adreno.so", "vkQueuePresentKHR");
            LOGI("[EGLHook] ELF resolve(vulkan.adreno.so, vkQueuePresentKHR) = %p", vkPresent);
        }

        if (vkPresent) {
            // Create EGL overlay BEFORE hooking (so it's ready when first frame arrives)
            CreateVulkanEGLOverlay();

            void* orig = nullptr;
            int ret = DobbyHook(vkPresent, reinterpret_cast<void*>(Hooked_vkQueuePresentKHR), &orig);
            if (ret == 0) {
                Orig_vkQueuePresentKHR = reinterpret_cast<vkQueuePresentKHR_t>(orig);
                successCount++;
                LOGI("[EGLHook] SUCCESS: Hooked vkQueuePresentKHR at %p", vkPresent);
            } else {
                LOGI("[EGLHook] FAILED: Could not hook vkQueuePresentKHR at %p (ret=%d)", vkPresent, ret);
            }
        } else {
            LOGI("[EGLHook] vkQueuePresentKHR not found (game may not use Vulkan)");
        }

        LOGI("[EGLHook] === Hook initialization complete: %d total hooks installed ===", successCount);

        // ======== 4. Watchdog ========
        std::thread([this]() {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            uint64_t eglFrames = gFrameCounter.load();
            uint64_t vkFrames = gVkFrameCounter.load();
            LOGI("[Watchdog] === 5-second diagnostics ===");
            LOGI("[Watchdog] EGL frames: %llu", (unsigned long long)eglFrames);
            LOGI("[Watchdog] Vulkan frames: %llu", (unsigned long long)vkFrames);
            if (eglFrames == 0 && vkFrames == 0) {
                LOGI("[Watchdog] *** ZERO frames from BOTH EGL and Vulkan! ***");
                LOGI("[Watchdog] *** The game may use a non-standard rendering path ***");
            } else if (vkFrames > 0 && eglFrames == 0) {
                LOGI("[Watchdog] >>> Game confirmed VULKAN rendering <<<");
            } else if (eglFrames > 0) {
                LOGI("[Watchdog] >>> Game confirmed EGL/GLES rendering <<<");
            }
        }).detach();

        bInitialized = (successCount > 0);
        LOGI("[EGLHook] Initialize() returning %s", bInitialized ? "true" : "false");
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
