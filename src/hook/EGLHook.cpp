#include "EGLHook.hpp"
#include "HookManager.hpp"
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

namespace Hook {

    using eglSwapBuffers_t = EGLBoolean (*)(EGLDisplay, EGLSurface);
    static eglSwapBuffers_t Orig_eglSwapBuffers = nullptr;

    using eglSwapBuffersDamage_t = EGLBoolean (*)(EGLDisplay, EGLSurface, EGLint*, EGLint);
    static eglSwapBuffersDamage_t Orig_eglSwapBuffersWithDamageKHR = nullptr;

    static std::atomic<uint64_t> gFrameCounter{0};

    // Central render entry point - called from ANY hooked swap function
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

    // Try to hook a single function, deduplicating by address
    static int TryHookUnique(void* addr, void* hookFn, void** origOut, 
                              std::vector<void*>& hookedAddrs, const char* desc) {
        if (!addr) return 0;
        // Deduplicate: don't hook same address twice
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
            if (origOut && !*origOut && orig) {
                *origOut = orig;
            }
            LOGI("[EGLHook] SUCCESS: Hooked %s at %p -> trampoline %p", desc, addr, orig);
            return 1;
        } else {
            LOGI("[EGLHook] FAILED: Could not hook %s at %p (ret=%d)", desc, addr, ret);
            return 0;
        }
    }

    bool EGLHook::Initialize() {
        if (bInitialized) return true;

        LOGI("[EGLHook] === Starting comprehensive graphics hook initialization ===");

        std::vector<void*> hookedAddrs;
        int successCount = 0;

        // ============================================================
        // 1. Hook eglSwapBuffers from RTLD_DEFAULT
        // ============================================================
        void* defaultSwap = dlsym(RTLD_DEFAULT, "eglSwapBuffers");
        LOGI("[EGLHook] dlsym(RTLD_DEFAULT, eglSwapBuffers) = %p", defaultSwap);
        successCount += TryHookUnique(defaultSwap, 
            reinterpret_cast<void*>(Hooked_eglSwapBuffers),
            reinterpret_cast<void**>(&Orig_eglSwapBuffers), hookedAddrs, "RTLD_DEFAULT::eglSwapBuffers");

        // ============================================================
        // 2. Hook eglSwapBuffers from libEGL.so
        // ============================================================
        void* eglHandle = dlopen("libEGL.so", RTLD_NOW);
        LOGI("[EGLHook] dlopen(libEGL.so) = %p", eglHandle);
        if (eglHandle) {
            void* swap = dlsym(eglHandle, "eglSwapBuffers");
            LOGI("[EGLHook] dlsym(libEGL.so, eglSwapBuffers) = %p", swap);
            successCount += TryHookUnique(swap,
                reinterpret_cast<void*>(Hooked_eglSwapBuffers),
                reinterpret_cast<void**>(&Orig_eglSwapBuffers), hookedAddrs, "libEGL.so::eglSwapBuffers");

            void* swapDmg = dlsym(eglHandle, "eglSwapBuffersWithDamageKHR");
            LOGI("[EGLHook] dlsym(libEGL.so, eglSwapBuffersWithDamageKHR) = %p", swapDmg);
            successCount += TryHookUnique(swapDmg,
                reinterpret_cast<void*>(Hooked_eglSwapBuffersWithDamageKHR),
                reinterpret_cast<void**>(&Orig_eglSwapBuffersWithDamageKHR), hookedAddrs, "libEGL.so::eglSwapBuffersWithDamageKHR");
        }

        // ============================================================
        // 3. Scan /proc/self/maps for GPU driver libraries
        // ============================================================
        FILE* fp = fopen("/proc/self/maps", "rt");
        if (fp) {
            char line[512];
            std::vector<std::string> checkedLibs;
            LOGI("[EGLHook] Scanning /proc/self/maps for GPU driver libraries...");
            while (fgets(line, sizeof(line), fp)) {
                // Match any graphics-related .so
                if (!strstr(line, ".so")) continue;
                bool isGraphics = strstr(line, "EGL") || strstr(line, "GLES") ||
                                  strstr(line, "egl") || strstr(line, "gles") ||
                                  strstr(line, "adreno") || strstr(line, "mali") || 
                                  strstr(line, "vulkan") || strstr(line, "gpu") ||
                                  strstr(line, "GL") || strstr(line, "powervr");
                if (!isGraphics) continue;

                char* soPath = strstr(line, "/");
                if (!soPath) continue;
                
                char cleanPath[256] = {0};
                sscanf(soPath, "%255s", cleanPath);
                
                // Deduplicate
                bool seen = false;
                for (const auto& s : checkedLibs) {
                    if (s == cleanPath) { seen = true; break; }
                }
                if (seen) continue;
                checkedLibs.push_back(cleanPath);

                void* h = dlopen(cleanPath, RTLD_NOLOAD | RTLD_NOW);
                if (!h) h = dlopen(cleanPath, RTLD_NOW);
                if (!h) {
                    LOGI("[EGLHook] Cannot dlopen driver: %s", cleanPath);
                    continue;
                }

                LOGI("[EGLHook] Probing driver: %s (handle=%p)", cleanPath, h);

                // Try eglSwapBuffers
                void* fn = dlsym(h, "eglSwapBuffers");
                if (fn) {
                    LOGI("[EGLHook]   -> found eglSwapBuffers at %p", fn);
                    char desc[300];
                    snprintf(desc, sizeof(desc), "%s::eglSwapBuffers", cleanPath);
                    successCount += TryHookUnique(fn,
                        reinterpret_cast<void*>(Hooked_eglSwapBuffers),
                        reinterpret_cast<void**>(&Orig_eglSwapBuffers), hookedAddrs, desc);
                }

                // Try eglSwapBuffersWithDamageKHR
                void* fnDmg = dlsym(h, "eglSwapBuffersWithDamageKHR");
                if (fnDmg) {
                    LOGI("[EGLHook]   -> found eglSwapBuffersWithDamageKHR at %p", fnDmg);
                    char desc[300];
                    snprintf(desc, sizeof(desc), "%s::eglSwapBuffersWithDamageKHR", cleanPath);
                    successCount += TryHookUnique(fnDmg,
                        reinterpret_cast<void*>(Hooked_eglSwapBuffersWithDamageKHR),
                        reinterpret_cast<void**>(&Orig_eglSwapBuffersWithDamageKHR), hookedAddrs, desc);
                }

                // Try eglSwapBuffersWithDamageEXT
                void* fnDmgExt = dlsym(h, "eglSwapBuffersWithDamageEXT");
                if (fnDmgExt) {
                    LOGI("[EGLHook]   -> found eglSwapBuffersWithDamageEXT at %p", fnDmgExt);
                    char desc[300];
                    snprintf(desc, sizeof(desc), "%s::eglSwapBuffersWithDamageEXT", cleanPath);
                    successCount += TryHookUnique(fnDmgExt,
                        reinterpret_cast<void*>(Hooked_eglSwapBuffersWithDamageKHR),
                        reinterpret_cast<void**>(&Orig_eglSwapBuffersWithDamageKHR), hookedAddrs, desc);
                }
            }
            fclose(fp);
        }

        LOGI("[EGLHook] === Hook initialization complete: %d hooks installed ===", successCount);

        // ============================================================
        // 4. Start a watchdog thread to detect if frames are arriving
        // ============================================================
        std::thread([this]() {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            uint64_t frames = gFrameCounter.load();
            if (frames == 0) {
                LOGI("[EGLHook] *** WARNING: After 5 seconds, ZERO frames detected! ***");
                LOGI("[EGLHook] *** This means eglSwapBuffers is NOT being called by the game ***");
                LOGI("[EGLHook] *** The game likely uses Vulkan rendering (vkQueuePresentKHR) ***");
                LOGI("[EGLHook] *** or a non-standard presentation path ***");
                
                // Dump all loaded .so files for debugging
                FILE* mf = fopen("/proc/self/maps", "rt");
                if (mf) {
                    char mline[512];
                    std::vector<std::string> dumpedLibs;
                    LOGI("[EGLHook] === Dumping all loaded graphics-related libraries ===");
                    while (fgets(mline, sizeof(mline), mf)) {
                        if (!strstr(mline, ".so")) continue;
                        char* sp = strstr(mline, "/");
                        if (!sp) continue;
                        char cp[256] = {0};
                        sscanf(sp, "%255s", cp);
                        bool dup = false;
                        for (const auto& d : dumpedLibs) { if (d == cp) { dup = true; break; } }
                        if (dup) continue;
                        dumpedLibs.push_back(cp);
                        // Only log graphics-related ones
                        if (strstr(cp, "EGL") || strstr(cp, "egl") || strstr(cp, "GL") || 
                            strstr(cp, "gl") || strstr(cp, "vulkan") || strstr(cp, "Vulkan") ||
                            strstr(cp, "adreno") || strstr(cp, "mali") || strstr(cp, "gpu") ||
                            strstr(cp, "render") || strstr(cp, "libUE") || strstr(cp, "Unreal")) {
                            LOGI("[EGLHook]   LOADED: %s", cp);
                        }
                    }
                    fclose(mf);
                }
            } else {
                LOGI("[EGLHook] Watchdog OK: %llu frames rendered in first 5 seconds", (unsigned long long)frames);
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
                io.DisplaySize = ImVec2(static_cast<float>(ScreenWidth), static_cast<float>(ScreenHeight));

                ImGui_ImplOpenGL3_Init("#version 300 es");
                GUI::MainGUI::Get().Initialize();

                bImGuiInitialized = true;
                LOGI("[EGLHook] >>> ImGui Overlay Initialized! Screen: %dx%d <<<", ScreenWidth, ScreenHeight);
            } else {
                LOGI("[EGLHook] eglQuerySurface returned invalid size: %dx%d", ScreenWidth, ScreenHeight);
            }
        }

        if (bImGuiInitialized) {
            // Save OpenGL ES state
            GLint last_program, last_active_texture, last_array_buffer, last_element_array_buffer, last_vertex_array;
            glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
            glGetIntegerv(GL_ACTIVE_TEXTURE, &last_active_texture);
            glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_array_buffer);
            glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &last_element_array_buffer);
            glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vertex_array);

            GLint last_viewport[4], last_scissor_box[4];
            glGetIntegerv(GL_VIEWPORT, last_viewport);
            glGetIntegerv(GL_SCISSOR_BOX, last_scissor_box);
            GLboolean last_enable_blend = glIsEnabled(GL_BLEND);
            GLboolean last_enable_cull_face = glIsEnabled(GL_CULL_FACE);
            GLboolean last_enable_depth_test = glIsEnabled(GL_DEPTH_TEST);
            GLboolean last_enable_scissor_test = glIsEnabled(GL_SCISSOR_TEST);

            // Update display size
            eglQuerySurface(dpy, surface, EGL_WIDTH, &ScreenWidth);
            eglQuerySurface(dpy, surface, EGL_HEIGHT, &ScreenHeight);
            ImGuiIO& io = ImGui::GetIO();
            io.DisplaySize = ImVec2(static_cast<float>(ScreenWidth), static_cast<float>(ScreenHeight));

            ImGui_ImplOpenGL3_NewFrame();
            ImGui::NewFrame();

            GUI::MainGUI::Get().Render();

            ImGui::Render();
            glViewport(0, 0, ScreenWidth, ScreenHeight);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            // Restore OpenGL ES state
            glUseProgram(last_program);
            glBindTexture(GL_TEXTURE_2D, last_active_texture);
            glBindVertexArray(last_vertex_array);
            glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_element_array_buffer);
            glViewport(last_viewport[0], last_viewport[1], (GLsizei)last_viewport[2], (GLsizei)last_viewport[3]);
            glScissor(last_scissor_box[0], last_scissor_box[1], (GLsizei)last_scissor_box[2], (GLsizei)last_scissor_box[3]);
            if (last_enable_blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
            if (last_enable_cull_face) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
            if (last_enable_depth_test) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
            if (last_enable_scissor_test) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
        }

        return EGL_TRUE;
    }
}
