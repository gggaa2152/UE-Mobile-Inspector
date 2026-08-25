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
#include <GLES3/gl3.h>

namespace Hook {

    using eglSwapBuffers_t = EGLBoolean (*)(EGLDisplay, EGLSurface);
    static eglSwapBuffers_t Orig_eglSwapBuffers = nullptr;

    using eglGetProcAddress_t = void* (*)(const char*);
    static eglGetProcAddress_t Orig_eglGetProcAddress = nullptr;

    static uint64_t gFrameCounter = 0;

    static EGLBoolean Hooked_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
        gFrameCounter++;
        if (gFrameCounter == 1 || gFrameCounter % 300 == 0) {
            LOGI("[EGLHook] Frame #%llu Render Loop Active (Display: %p, Surface: %p)", 
                 (unsigned long long)gFrameCounter, dpy, surface);
        }

        EGLHook::Get().OnSwapBuffers(dpy, surface);
        return Orig_eglSwapBuffers ? Orig_eglSwapBuffers(dpy, surface) : EGL_TRUE;
    }

    static void* Hooked_eglGetProcAddress(const char* procname) {
        if (procname && strcmp(procname, "eglSwapBuffers") == 0) {
            return reinterpret_cast<void*>(Hooked_eglSwapBuffers);
        }
        return Orig_eglGetProcAddress ? Orig_eglGetProcAddress(procname) : nullptr;
    }

    bool EGLHook::Initialize() {
        if (bInitialized) return true;

        std::vector<void*> targetAddrs;

        // 1. Resolve from RTLD_DEFAULT & libEGL.so
        void* defaultSwap = dlsym(RTLD_DEFAULT, "eglSwapBuffers");
        if (defaultSwap) {
            targetAddrs.push_back(defaultSwap);
            LOGI("[EGLHook] Resolved default eglSwapBuffers at: %p", defaultSwap);
        }

        void* eglHandle = dlopen("libEGL.so", RTLD_NOW);
        if (eglHandle) {
            void* swap = dlsym(eglHandle, "eglSwapBuffers");
            if (swap) {
                targetAddrs.push_back(swap);
                LOGI("[EGLHook] Resolved libEGL.so eglSwapBuffers at: %p", swap);
            }
        }

        // 2. Scan /proc/self/maps for all active GPU driver libraries (Adreno, Mali, GLES, Vulkan)
        FILE* fp = fopen("/proc/self/maps", "rt");
        if (fp) {
            char line[512];
            std::vector<std::string> checkedLibs;
            while (fgets(line, sizeof(line), fp)) {
                if (strstr(line, ".so") && (strstr(line, "EGL") || strstr(line, "GLES") || 
                    strstr(line, "adreno") || strstr(line, "mali") || strstr(line, "vulkan"))) {
                    char* soPath = strstr(line, "/");
                    if (soPath) {
                        char cleanPath[256] = {0};
                        sscanf(soPath, "%255s", cleanPath);
                        bool seen = false;
                        for (const auto& s : checkedLibs) {
                            if (s == cleanPath) { seen = true; break; }
                        }
                        if (!seen) {
                            checkedLibs.push_back(cleanPath);
                            void* h = dlopen(cleanPath, RTLD_NOLOAD | RTLD_NOW);
                            if (!h) h = dlopen(cleanPath, RTLD_NOW);
                            if (h) {
                                void* fn = dlsym(h, "eglSwapBuffers");
                                if (fn) {
                                    targetAddrs.push_back(fn);
                                    LOGI("[EGLHook] Found Driver eglSwapBuffers in %s at %p", cleanPath, fn);
                                }
                                void* fnDamage = dlsym(h, "eglSwapBuffersWithDamageKHR");
                                if (fnDamage) {
                                    targetAddrs.push_back(fnDamage);
                                    LOGI("[EGLHook] Found Driver eglSwapBuffersWithDamageKHR in %s at %p", cleanPath, fnDamage);
                                }
                            }
                        }
                    }
                }
            }
            fclose(fp);
        }

        // 3. Hook eglGetProcAddress so engine queries also redirect
        void* gpa = dlsym(RTLD_DEFAULT, "eglGetProcAddress");
        if (gpa) {
            DobbyHook(gpa, reinterpret_cast<void*>(Hooked_eglGetProcAddress), reinterpret_cast<void**>(&Orig_eglGetProcAddress));
            LOGI("[EGLHook] Hooked eglGetProcAddress at %p", gpa);
        }

        // 4. Hook all resolved SwapBuffers functions
        int successCount = 0;
        for (void* addr : targetAddrs) {
            if (!addr) continue;
            void* orig = nullptr;
            int ret = DobbyHook(addr, reinterpret_cast<void*>(Hooked_eglSwapBuffers), &orig);
            if (ret == 0) {
                if (!Orig_eglSwapBuffers && orig) {
                    Orig_eglSwapBuffers = reinterpret_cast<eglSwapBuffers_t>(orig);
                }
                successCount++;
                LOGI("[EGLHook] Successfully hooked SwapBuffers at %p (orig=%p)", addr, orig);
            }
        }

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
                io.DisplaySize = ImVec2(static_cast<float>(ScreenWidth), static_cast<float>(ScreenHeight));

                ImGui_ImplOpenGL3_Init("#version 300 es");
                GUI::MainGUI::Get().Initialize();

                bImGuiInitialized = true;
                LOGI("[EGLHook] >>> ImGui Overlay Initialized on Screen (%dx%d)! <<<", ScreenWidth, ScreenHeight);
            } else {
                LOGI("[EGLHook] eglQuerySurface returned invalid dimensions: %dx%d", ScreenWidth, ScreenHeight);
            }
        }

        if (bImGuiInitialized) {
            // Save current OpenGL ES state
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

            // Render Our Tool UI & Floating Ball
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
