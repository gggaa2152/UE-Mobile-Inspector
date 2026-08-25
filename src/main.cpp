#include <jni.h>
#include <thread>
#include <chrono>
#include <android/log.h>
#include "config.hpp"
#include "core/Memory.hpp"
#include "core/UECore.hpp"
#include "hook/EGLHook.hpp"

#define LOG_TAG "UE-Mobile-Inspector"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

static void MainThread() {
    LOGI("==========================================");
    LOGI(" %s Loaded! Initializing...", TOOL_TAG);
    LOGI("==========================================");

    // Wait until libUE4.so and libEGL.so are fully loaded in memory
    while (!Memory::GetModuleBase(Config::UE_SO_NAME) || !Memory::GetModuleBase("libEGL.so")) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    LOGI("Found Unreal Engine module and EGL. Hooking graphics pipeline...");

    // 1. Hook EGL SwapBuffers to render ImGui overlay
    if (Hook::EGLHook::Get().Initialize()) {
        LOGI("EGL Overlay hooked successfully!");
    }

    // 2. Initialize Reflection Engine
    if (UE::CoreManager::Get().Initialize()) {
        LOGI("UE Reflection Engine initialized successfully!");
    }
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
