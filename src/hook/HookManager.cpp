#include "HookManager.hpp"
#include <sys/mman.h>
#include <unistd.h>
#include <cstdlib>

namespace Hook {

    bool HookEngine::SetMemoryWritable(void* addr, size_t size) {
        uintptr_t pageSize = static_cast<uintptr_t>(sysconf(_SC_PAGESIZE));
        uintptr_t pageStart = reinterpret_cast<uintptr_t>(addr) & ~(pageSize - 1);
        uintptr_t pageEnd = (reinterpret_cast<uintptr_t>(addr) + size + pageSize - 1) & ~(pageSize - 1);
        return mprotect(reinterpret_cast<void*>(pageStart), pageEnd - pageStart, PROT_READ | PROT_WRITE | PROT_EXEC) == 0;
    }

    bool HookEngine::Hook(void* target, void* replace, void** origin) {
        if (!target || !replace) return false;

#if defined(__aarch64__)
        // ARM64 Architecture Hook (16 bytes)
        // 0x58000050: LDR X16, #8
        // 0xD61F0200: BR X16
        // [8-byte Address]
        constexpr size_t HookSize = 16;
        
        HookRecord record;
        record.Target = target;
        record.OrigSize = HookSize;
        memcpy(record.OrigBytes, target, HookSize);

        // Allocate executable memory for origin trampoline
        void* trampoline = mmap(nullptr, 64, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        if (trampoline == MAP_FAILED) {
            return false;
        }

        uint8_t* trampBytes = static_cast<uint8_t*>(trampoline);
        // 1. Copy original 16 bytes
        memcpy(trampBytes, target, HookSize);

        // 2. Add jump back to target + 16
        uint32_t trampJump[2] = { 0x58000050, 0xD61F0200 };
        uint64_t returnAddr = reinterpret_cast<uint64_t>(target) + HookSize;
        memcpy(trampBytes + HookSize, trampJump, sizeof(trampJump));
        memcpy(trampBytes + HookSize + 8, &returnAddr, sizeof(uint64_t));

        __builtin___clear_cache(reinterpret_cast<char*>(trampBytes), reinterpret_cast<char*>(trampBytes + 32));

        if (origin) {
            *origin = trampoline;
        }

        // Apply hook on target
        if (!SetMemoryWritable(target, HookSize)) {
            munmap(trampoline, 64);
            return false;
        }

        uint32_t hookCode[2] = { 0x58000050, 0xD61F0200 };
        uint64_t targetDest = reinterpret_cast<uint64_t>(replace);
        
        memcpy(target, hookCode, sizeof(hookCode));
        memcpy(static_cast<uint8_t*>(target) + 8, &targetDest, sizeof(uint64_t));

        __builtin___clear_cache(reinterpret_cast<char*>(target), reinterpret_cast<char*>(target) + HookSize);

        record.Trampoline = trampoline;
        Hooks.push_back(record);
        return true;

#elif defined(__arm__)
        // ARMv7 (32-bit) Architecture Hook (8 bytes)
        // 0xE51FF004: LDR PC, [PC, #-4]
        // [4-byte Address]
        constexpr size_t HookSize = 8;

        HookRecord record;
        record.Target = target;
        record.OrigSize = HookSize;
        memcpy(record.OrigBytes, target, HookSize);

        void* trampoline = mmap(nullptr, 32, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        if (trampoline == MAP_FAILED) {
            return false;
        }

        uint8_t* trampBytes = static_cast<uint8_t*>(trampoline);
        memcpy(trampBytes, target, HookSize);

        uint32_t trampJump = 0xE51FF004;
        uint32_t returnAddr = reinterpret_cast<uint32_t>(target) + HookSize;
        memcpy(trampBytes + HookSize, &trampJump, sizeof(uint32_t));
        memcpy(trampBytes + HookSize + 4, &returnAddr, sizeof(uint32_t));

        __builtin___clear_cache(reinterpret_cast<char*>(trampBytes), reinterpret_cast<char*>(trampBytes + 16));

        if (origin) {
            *origin = trampoline;
        }

        if (!SetMemoryWritable(target, HookSize)) {
            munmap(trampoline, 32);
            return false;
        }

        uint32_t hookCode = 0xE51FF004;
        uint32_t targetDest = reinterpret_cast<uint32_t>(replace);

        memcpy(target, &hookCode, sizeof(uint32_t));
        memcpy(static_cast<uint8_t*>(target) + 4, &targetDest, sizeof(uint32_t));

        __builtin___clear_cache(reinterpret_cast<char*>(target), reinterpret_cast<char*>(target) + HookSize);

        record.Trampoline = trampoline;
        Hooks.push_back(record);
        return true;
#else
        return false;
#endif
    }

    bool HookEngine::Unhook(void* target) {
        for (auto it = Hooks.begin(); it != Hooks.end(); ++it) {
            if (it->Target == target) {
                if (SetMemoryWritable(target, it->OrigSize)) {
                    memcpy(target, it->OrigBytes, it->OrigSize);
                    __builtin___clear_cache(reinterpret_cast<char*>(target), reinterpret_cast<char*>(target) + it->OrigSize);
                }
                if (it->Trampoline) {
                    munmap(it->Trampoline, 64);
                }
                Hooks.erase(it);
                return true;
            }
        }
        return false;
    }
}
