#include <jni.h>
#include <thread>
#include <chrono>
#include "config.hpp"
#include "core/Logger.hpp"
#include "core/Memory.hpp"
#include "core/UECore.hpp"
#include "hook/EGLHook.hpp"

static void MainThread() {
    LOGI("==========================================");
    LOGI(" %s Loaded! Initializing...", TOOL_TAG);
    LOGI("==========================================");

    // 1. Hook EGL SwapBuffers immediately in a retry loop so the UI/floating button displays ASAP
    std::thread([]() {
        int retries = 0;
        LOGI("Starting Graphics Presentation (EGL/GLES) hook loop...");
        while (!Hook::EGLHook::Get().Initialize() && retries++ < 120) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        LOGI("Graphics Hook Status: %s (after %d attempts)", 
             Hook::EGLHook::Get().IsInitialized() ? "SUCCESS" : "RETRYING", retries);
    }).detach();

    // 2. Scan and initialize Unreal Engine reflection engine
    std::thread([]() {
        const char* potentialNames[] = {
            "libUE4.so", "libUE5.so", "libUnreal.so", "libShadowTrackerExtra.so", 
            "libdfm.so", "libgame.so", "libmain.so", "libGVoice.so"
        };

        LOGI("Scanning memory for Unreal Engine modules...");
        while (true) {
            for (const char* name : potentialNames) {
                if (Memory::GetModuleBase(name)) {
                    LOGI("Detected Unreal Engine module: %s", name);
                    Config::UE_SO_NAME = name;
                    if (UE::CoreManager::Get().Initialize()) {
                        LOGI("UE Reflection Engine successfully initialized for %s!", name);
                        return;
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }).detach();
}

// Android Dynamic Library Constructor
__attribute__((constructor))
void InitLibrary() {
    std::thread(MainThread).detach();
}

// JNI Entry point if loaded via Java System.loadLibrary
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    std::thread(MainThread).detach();
    return JNI_VERSION_1_6;
}
