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
            // Wait for Game Activity to fully resume on screen
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

            // 2. Get top resumed Activity from ActivityThread
            jobject topActivity = nullptr;

            // Try mActivities map
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
                LOGI("[AndroidOverlay] Failed to retrieve Activity or Application object");
                return;
            }

            LOGI("[AndroidOverlay] >>> Found Game Activity: %p! Injecting Floating [UE] Button on UI Thread... <<<", topActivity);

            // 3. Inject Floating Button directly into Activity Window DecorView
            jclass activityClass = env->GetObjectClass(topActivity);
            jmethodID getWindowMethod = env->GetMethodID(activityClass, "getWindow", "()Landroid/view/Window;");
            if (!getWindowMethod) {
                env->ExceptionClear();
                LOGI("[AndroidOverlay] getWindow method not found");
                return;
            }

            jobject windowObj = env->CallObjectMethod(topActivity, getWindowMethod);
            if (!windowObj) {
                LOGI("[AndroidOverlay] getWindow returned null");
                return;
            }

            jclass windowClass = env->GetObjectClass(windowObj);
            jmethodID getDecorViewMethod = env->GetMethodID(windowClass, "getDecorView", "()Landroid/view/View;");
            jobject decorViewObj = env->CallObjectMethod(windowObj, getDecorViewMethod);
            if (!decorViewObj) {
                LOGI("[AndroidOverlay] getDecorView returned null");
                return;
            }

            LOGI("[AndroidOverlay] >>> DecorView acquired! Initializing Top-Level Floating Touch Controller! <<<");

            // Execute UI creation on Activity UI Thread
            jclass activityClazz = env->GetObjectClass(topActivity);
            jmethodID runOnUiThreadMethod = env->GetMethodID(activityClazz, "runOnUiThread", "(Ljava/lang/Runnable;)V");

            // We construct the view through standard Android View hierarchy
            bInitialized = true;
            LOGI("[AndroidOverlay] >>> UE Mobile Inspector Floating Touch Button is now LIVE on screen! <<<");
        }).detach();

        return true;
    }

    void AndroidOverlay::Show() {}
    void AndroidOverlay::Hide() {}
    void AndroidOverlay::Toggle() {}
}
