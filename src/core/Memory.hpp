#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <sys/mman.h>
#include <unistd.h>

namespace Memory {
    struct SegmentInfo {
        uintptr_t start;
        uintptr_t end;
        bool isReadable;
        bool isWritable;
        bool isExecutable;
    };

    uintptr_t GetModuleBase(const char* moduleName);
    size_t GetModuleSize(const char* moduleName);
    std::vector<SegmentInfo> GetModuleSegments(const char* moduleName);
    bool IsAddressInExecutable(uintptr_t addr, const char* moduleName = nullptr);
    
    // Pattern scanning (AOB Signature scan)
    uintptr_t FindPattern(const char* moduleName, const char* pattern, const char* mask);
    uintptr_t FindPattern(uintptr_t start, size_t length, const char* pattern, const char* mask);

    // Safe memory reading / validation
    bool IsValidPtr(const void* ptr);
    
    template <typename T>
    T Read(uintptr_t address, T defaultValue = T()) {
        if (!IsValidPtr(reinterpret_cast<void*>(address))) {
            return defaultValue;
        }
        return *reinterpret_cast<T*>(address);
    }

    template <typename T>
    bool Write(uintptr_t address, const T& value) {
        if (!IsValidPtr(reinterpret_cast<void*>(address))) {
            return false;
        }
        
        // Ensure write protection is lifted
        size_t pageSize = getpagesize();
        uintptr_t pageStart = address & ~(pageSize - 1);
        mprotect(reinterpret_cast<void*>(pageStart), pageSize * 2, PROT_READ | PROT_WRITE | PROT_EXEC);
        
        *reinterpret_cast<T*>(address) = value;
        return true;
    }
}
