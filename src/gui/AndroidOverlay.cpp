#include "AndroidOverlay.hpp"
#include "core/Logger.hpp"
#include "core/UECore.hpp"
#include "core/Memory.hpp"
#include <dlfcn.h>
#include <thread>
#include <chrono>

typedef jint (*JNI_GetCreatedJavaVMs_t)(JavaVM**, jsize, jsize*);

namespace GUI {

    static JavaVM* gSavedVM = nullptr;

    bool AndroidOverlay::Initialize(JavaVM* vm) {
        if (bInitialized) return true;
        if (vm) gJavaVM = vm;

        if (!gJavaVM) {
            void* artHandle = dlopen("libart.so", RTLD_NOW);
            if (!artHandle) artHandle = dlopen("libnativehelper.so", RTLD_NOW);
            if (artHandle) {
                auto getVMs = reinterpret_cast<JNI_GetCreatedJavaVMs_t>(dlsym(artHandle, "JNI_GetCreatedJavaVMs"));
                if (getVMs) {
                    jsize count = 0;
                    getVMs(&gJavaVM, 1, &count);
                    if (count > 0 && gJavaVM) {
                        LOGI("[AndroidOverlay] Successfully acquired JavaVM from runtime!");
                    }
                }
            }
        }

        if (!gJavaVM) {
            LOGI("[AndroidOverlay] JavaVM not ready yet, will retry...");
            return false;
        }

        gSavedVM = gJavaVM;

        std::thread([this]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(1500));
            JNIEnv* env = nullptr;
            if (gSavedVM->AttachCurrentThread(&env, nullptr) != JNI_OK || !env) {
                LOGI("[AndroidOverlay] Failed to attach current thread to JVM");
                return;
            }

            // Find ActivityThread
            jclass activityThreadClass = env->FindClass("android/app/ActivityThread");
            if (!activityThreadClass) {
                env->ExceptionClear();
                LOGI("[AndroidOverlay] ActivityThread class not found");
                return;
            }

            jmethodID currentActivityThread = env->GetStaticMethodID(activityThreadClass, "currentActivityThread", "()Landroid/app/ActivityThread;");
            jobject actThread = env->CallStaticObjectMethod(activityThreadClass, currentActivityThread);
            if (!actThread) {
                LOGI("[AndroidOverlay] currentActivityThread returned null");
                return;
            }

            LOGI("[AndroidOverlay] >>> Android ActivityThread attached! Initializing in-window Floating UI... <<<");
            bInitialized = true;
        }).detach();

        return true;
    }

    void AndroidOverlay::Show() {}
    void AndroidOverlay::Hide() {}
    void AndroidOverlay::Toggle() {}
}
