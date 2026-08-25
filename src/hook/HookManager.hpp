#pragma once

#include <cstdint>
#include <cstddef>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <vector>

namespace Hook {

    // Ultra-reliable, self-contained Android ARM64 & ARMv7 Inline Hook Engine
    class HookEngine {
    public:
        static HookEngine& Get() {
            static HookEngine instance;
            return instance;
        }

        bool Hook(void* target, void* replace, void** origin);
        bool Unhook(void* target);

    private:
        HookEngine() = default;

        struct HookRecord {
            void* Target;
            void* Trampoline;
            uint8_t OrigBytes[32];
            size_t OrigSize;
        };

        std::vector<HookRecord> Hooks;
        bool SetMemoryWritable(void* addr, size_t size);
    };

    inline bool InlineHook(void* target, void* replace, void** origin) {
        return HookEngine::Get().Hook(target, replace, origin);
    }
}

// C-style Dobby-compatible alias
extern "C" {
    inline int DobbyHook(void* target, void* replace, void** origin) {
        return Hook::InlineHook(target, replace, origin) ? 0 : -1;
    }
    inline int DobbyDestroy(void* target) {
        return Hook::HookEngine::Get().Unhook(target) ? 0 : -1;
    }
}
