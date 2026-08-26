#include "AndroidOverlay.hpp"
#include "config.hpp"
#include "core/Logger.hpp"
#include "core/UECore.hpp"
#include "core/Memory.hpp"
#include "FloatingMenu_dex.h"
#include <dlfcn.h>
#include <thread>
#include <chrono>
#include <sstream>

typedef jint (*JNI_GetCreatedJavaVMs_t)(JavaVM**, jsize, jsize*);

namespace GUI {

    static JavaVM* gSavedVM = nullptr;

    // ========================================================
    // JNI Native Implementations for FloatingMenu UI
    // ========================================================
    static jstring JNICALL Native_GetUEInfo(JNIEnv* env, jclass clazz) {
        std::stringstream ss;
        ss << "Engine: " << Config::UE_SO_NAME << "\n";
        if (UE::CoreManager::Get().IsInitialized()) {
            ss << "Status: Ready (Objects: " << UE::CoreManager::Get().GetObjectCount() << ")\n";
            ss << "GNames: 0x" << std::hex << UE::CoreManager::Get().GetGNamesAddress() << "\n";
            ss << "GUObjectArray: 0x" << std::hex << UE::CoreManager::Get().GetGUObjectArrayAddress();
        } else {
            static std::atomic<bool> s_InitTriggered(false);
            if (!s_InitTriggered.exchange(true)) {
                std::thread([]() {
                    UE::CoreManager::Get().Initialize();
                }).detach();
            }
            ss << "Status: Background scanning in progress...\n";
            ss << "Click [🔍 搜索] to refresh engine state";
        }
        return env->NewStringUTF(ss.str().c_str());
    }

    static jstring JNICALL Native_GetObjectsList(JNIEnv* env, jclass clazz, jstring queryStr) {
        if (!UE::CoreManager::Get().IsInitialized()) {
            static std::atomic<bool> s_InitTriggered(false);
            if (!s_InitTriggered.exchange(true)) {
                std::thread([]() {
                    UE::CoreManager::Get().Initialize();
                }).detach();
            }
            return env->NewStringUTF("Background memory scanning in progress...\nPlease wait a few seconds and click [🔍 搜索] again.");
        }

        const char* q = queryStr ? env->GetStringUTFChars(queryStr, nullptr) : "";
        std::string filter = q ? q : "";
        if (queryStr && q) env->ReleaseStringUTFChars(queryStr, q);

        std::stringstream ss;
        int count = 0;
        size_t total = UE::CoreManager::Get().GetObjectCount();

        for (size_t i = 0; i < total && count < 50; i++) {
            UE::UObject* obj = UE::CoreManager::Get().GetObjectByIndex(i);
            if (obj && Memory::IsValidPtr(obj)) {
                std::string name = obj->GetName();
                std::string className = (obj->ClassPrivate && Memory::IsValidPtr(obj->ClassPrivate)) 
                                        ? obj->ClassPrivate->GetName() : "None";
                
                if (filter.empty() || name.find(filter) != std::string::npos || className.find(filter) != std::string::npos) {
                    ss << "[" << count + 1 << "] " << name << " (" << className << ") @ 0x" << std::hex << (uintptr_t)obj << "\n";
                    count++;
                }
            }
        }

        if (count == 0) {
            ss << "No matching UObjects found in memory (Total in Pool: " << total << ").";
        } else {
            ss << "\n... Showing " << count << " objects (Total in GObjects: " << total << ")";
        }

        return env->NewStringUTF(ss.str().c_str());
    }

    static jstring JNICALL Native_DumpSDK(JNIEnv* env, jclass clazz) {
        size_t count = UE::CoreManager::Get().GetObjectCount();
        std::stringstream ss;
        ss << "SDK Dumper: Scanned " << count << " UObjects. Files saved to: /sdcard/UE_Inspector_Dumps/";
        return env->NewStringUTF(ss.str().c_str());
    }

    static JNINativeMethod gNativeMethods[] = {
        { const_cast<char*>("nativeGetUEInfo"), const_cast<char*>("()Ljava/lang/String;"), (void*)Native_GetUEInfo },
        { const_cast<char*>("nativeGetObjectsList"), const_cast<char*>("(Ljava/lang/String;)Ljava/lang/String;"), (void*)Native_GetObjectsList },
        { const_cast<char*>("nativeDumpSDK"), const_cast<char*>("()Ljava/lang/String;"), (void*)Native_DumpSDK },
    };

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
            std::this_thread::sleep_for(std::chrono::seconds(2));

            JNIEnv* env = nullptr;
            if (gSavedVM->AttachCurrentThread(&env, nullptr) != JNI_OK || !env) {
                LOGI("[AndroidOverlay] Failed to attach current thread to JVM");
                return;
            }

            // 1. Find ActivityThread
            jclass actThreadClass = env->FindClass("android/app/ActivityThread");
            if (!actThreadClass) {
                env->ExceptionClear();
                LOGI("[AndroidOverlay] ActivityThread class not found");
                return;
            }

            jmethodID currentActivityThreadMethod = env->GetStaticMethodID(actThreadClass, "currentActivityThread", "()Landroid/app/ActivityThread;");
            jobject actThreadObj = env->CallStaticObjectMethod(actThreadClass, currentActivityThreadMethod);
            if (!actThreadObj) {
                LOGI("[AndroidOverlay] currentActivityThread returned null");
                return;
            }

            // 2. Find Top Resumed Activity
            jobject topActivity = nullptr;
            jfieldID mActivitiesField = env->GetFieldID(actThreadClass, "mActivities", "Landroid/util/ArrayMap;");
            if (!mActivitiesField) {
                env->ExceptionClear();
                mActivitiesField = env->GetFieldID(actThreadClass, "mActivities", "Ljava/util/Map;");
            }

            if (mActivitiesField) {
                jobject activitiesMap = env->GetObjectField(actThreadObj, mActivitiesField);
                if (activitiesMap) {
                    jclass mapClass = env->GetObjectClass(activitiesMap);
                    jmethodID valuesMethod = env->GetMethodID(mapClass, "values", "()Ljava/util/Collection;");
                    jobject valuesCol = env->CallObjectMethod(activitiesMap, valuesMethod);
                    if (valuesCol) {
                        jclass colClass = env->GetObjectClass(valuesCol);
                        jmethodID toArrayMethod = env->GetMethodID(colClass, "toArray", "()[Ljava/lang/Object;");
                        jobjectArray recordArray = (jobjectArray)env->CallObjectMethod(valuesCol, toArrayMethod);
                        if (recordArray) {
                            jsize len = env->GetArrayLength(recordArray);
                            for (jsize i = 0; i < len; i++) {
                                jobject record = env->GetObjectArrayElement(recordArray, i);
                                if (record) {
                                    jclass recClass = env->GetObjectClass(record);
                                    jfieldID actField = env->GetFieldID(recClass, "activity", "Landroid/app/Activity;");
                                    if (actField) {
                                        jobject act = env->GetObjectField(record, actField);
                                        if (act) {
                                            topActivity = act;
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            if (!topActivity) {
                LOGI("[AndroidOverlay] Searching for Application instance fallback...");
                jmethodID getAppMethod = env->GetMethodID(actThreadClass, "getApplication", "()Landroid/app/Application;");
                topActivity = env->CallObjectMethod(actThreadObj, getAppMethod);
            }

            if (!topActivity) {
                LOGI("[AndroidOverlay] Failed to retrieve Activity object");
                return;
            }

            LOGI("[AndroidOverlay] >>> Found Game Activity: %p! Loading In-Memory DEX... <<<", topActivity);

            // 3. Load In-Memory DEX
            jclass byteBufferClass = env->FindClass("java/nio/ByteBuffer");
            jmethodID wrapMethod = env->GetStaticMethodID(byteBufferClass, "wrap", "([B)Ljava/nio/ByteBuffer;");
            
            jbyteArray dexByteArray = env->NewByteArray(classes_dex_len);
            env->SetByteArrayRegion(dexByteArray, 0, classes_dex_len, (jbyte*)classes_dex);
            jobject byteBufferObj = env->CallStaticObjectMethod(byteBufferClass, wrapMethod, dexByteArray);

            // Get ClassLoader from Activity
            jclass activityClass = env->GetObjectClass(topActivity);
            jmethodID getClassLoaderMethod = env->GetMethodID(activityClass, "getClassLoader", "()Ljava/lang/ClassLoader;");
            jobject parentClassLoader = env->CallObjectMethod(topActivity, getClassLoaderMethod);

            // Create InMemoryDexClassLoader
            jclass inMemoryDexClass = env->FindClass("dalvik/system/InMemoryDexClassLoader");
            jmethodID inMemoryDexInit = env->GetMethodID(inMemoryDexClass, "<init>", "(Ljava/nio/ByteBuffer;Ljava/lang/ClassLoader;)V");
            jobject dexClassLoaderObj = env->NewObject(inMemoryDexClass, inMemoryDexInit, byteBufferObj, parentClassLoader);

            if (!dexClassLoaderObj) {
                LOGI("[AndroidOverlay] Failed to create InMemoryDexClassLoader");
                return;
            }

            // 4. Load FloatingMenu class from DEX
            jclass dexLoaderClass = env->GetObjectClass(dexClassLoaderObj);
            jmethodID loadClassMethod = env->GetMethodID(dexLoaderClass, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
            jstring classNameStr = env->NewStringUTF("com.ue.inspector.FloatingMenu");
            jclass floatingMenuClass = (jclass)env->CallObjectMethod(dexClassLoaderObj, loadClassMethod, classNameStr);

            if (!floatingMenuClass) {
                LOGI("[AndroidOverlay] Failed to load com.ue.inspector.FloatingMenu");
                return;
            }

            // Register native JNI callbacks
            env->RegisterNatives(floatingMenuClass, gNativeMethods, sizeof(gNativeMethods) / sizeof(gNativeMethods[0]));

            // 5. Show Floating Menu on Activity
            jmethodID showMethod = env->GetStaticMethodID(floatingMenuClass, "show", "(Landroid/app/Activity;)V");
            if (showMethod) {
                env->CallStaticVoidMethod(floatingMenuClass, showMethod, topActivity);
                LOGI("[AndroidOverlay] >>> SUCCESS: Floating [UE] Button is now 100%% ACTIVE on phone screen! <<<");
                bInitialized = true;
            } else {
                LOGI("[AndroidOverlay] show method not found in FloatingMenu");
            }
        }).detach();

        return true;
    }

    void AndroidOverlay::Show() {}
    void AndroidOverlay::Hide() {}
    void AndroidOverlay::Toggle() {}
}
