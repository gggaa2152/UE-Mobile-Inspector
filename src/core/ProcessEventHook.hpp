#pragma once

#include "UECore.hpp"
#include <string>
#include <vector>
#include <deque>
#include <mutex>
#include <chrono>

namespace UE {

    struct TraceRecord {
        std::chrono::system_clock::time_point Timestamp;
        std::string ObjectName;
        std::string ObjectClass;
        std::string FunctionName;
        uintptr_t ObjectAddr;
        uintptr_t FuncAddr;
    };

    class ProcessEventHook {
    public:
        static ProcessEventHook& Get() {
            static ProcessEventHook instance;
            return instance;
        }

        bool Enable();
        bool Disable();
        bool IsEnabled() const { return bEnabled; }

        void SetTracerActive(bool active) { bTracerActive = active; }
        bool IsTracerActive() const { return bTracerActive; }

        void SetFilter(const std::string& filter) { FilterStr = filter; }
        std::string GetFilter() const { return FilterStr; }

        void ClearRecords();
        std::vector<TraceRecord> GetRecords();

        // Called internally by the hooked ProcessEvent function
        void OnProcessEvent(UObject* Object, UFunction* Function, void* Parms);

    private:
        ProcessEventHook() = default;
        bool bEnabled = false;
        bool bTracerActive = false;
        std::string FilterStr = "";

        std::mutex RecordsMutex;
        std::deque<TraceRecord> Records;
        static constexpr size_t MaxRecords = 500;
    };
}
