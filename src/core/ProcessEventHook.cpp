#include "ProcessEventHook.hpp"
#include "HookManager.hpp"
#include <android/log.h>

#define LOG_TAG "UE-Mobile-Inspector"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

namespace UE {

    using ProcessEvent_t = void (*)(UObject*, UFunction*, void*);
    static ProcessEvent_t OrigProcessEvent = nullptr;

    static void Hooked_ProcessEvent(UObject* Object, UFunction* Function, void* Parms) {
        if (ProcessEventHook::Get().IsTracerActive()) {
            ProcessEventHook::Get().OnProcessEvent(Object, Function, Parms);
        }

        if (OrigProcessEvent) {
            OrigProcessEvent(Object, Function, Parms);
        }
    }

    bool ProcessEventHook::Enable() {
        if (bEnabled) return true;

        uintptr_t peAddr = CoreManager::Get().GetProcessEventAddress();
        if (!peAddr) {
            LOGI("ProcessEvent address not found, trying VTable hook fallback");
            return false;
        }

        int ret = DobbyHook(reinterpret_cast<void*>(peAddr), 
                            reinterpret_cast<void*>(Hooked_ProcessEvent), 
                            reinterpret_cast<void**>(&OrigProcessEvent));
        
        bEnabled = (ret == 0);
        LOGI("ProcessEvent Hook status: %s", bEnabled ? "Success" : "Failed");
        return bEnabled;
    }

    bool ProcessEventHook::Disable() {
        if (!bEnabled) return true;
        uintptr_t peAddr = CoreManager::Get().GetProcessEventAddress();
        if (peAddr) {
            DobbyDestroy(reinterpret_cast<void*>(peAddr));
        }
        bEnabled = false;
        return true;
    }

    void ProcessEventHook::ClearRecords() {
        std::lock_guard<std::mutex> lock(RecordsMutex);
        Records.clear();
    }

    std::vector<TraceRecord> ProcessEventHook::GetRecords() {
        std::lock_guard<std::mutex> lock(RecordsMutex);
        return std::vector<TraceRecord>(Records.begin(), Records.end());
    }

    void ProcessEventHook::OnProcessEvent(UObject* Object, UFunction* Function, void* Parms) {
        if (!Object || !Function || !Memory::IsValidPtr(Object) || !Memory::IsValidPtr(Function)) {
            return;
        }

        std::string funcName = Function->GetName();
        std::string objName = Object->GetName();
        std::string objClass = Object->ClassPrivate ? Object->ClassPrivate->GetName() : "None";

        // Apply filter if specified
        if (!FilterStr.empty()) {
            if (funcName.find(FilterStr) == std::string::npos &&
                objName.find(FilterStr) == std::string::npos &&
                objClass.find(FilterStr) == std::string::npos) {
                return;
            }
        }

        TraceRecord record;
        record.Timestamp = std::chrono::system_clock::now();
        record.ObjectName = objName;
        record.ObjectClass = objClass;
        record.FunctionName = funcName;
        record.ObjectAddr = reinterpret_cast<uintptr_t>(Object);
        record.FuncAddr = reinterpret_cast<uintptr_t>(Function);

        std::lock_guard<std::mutex> lock(RecordsMutex);
        if (Records.size() >= MaxRecords) {
            Records.pop_front();
        }
        Records.push_back(record);
    }
}
