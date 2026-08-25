#include "HookManager.hpp"
#include <sys/mman.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>

namespace Hook {

    bool HookEngine::SetMemoryWritable(void* addr, size_t size) {
        uintptr_t pageSize = static_cast<uintptr_t>(sysconf(_SC_PAGESIZE));
        uintptr_t pageStart = reinterpret_cast<uintptr_t>(addr) & ~(pageSize - 1);
        uintptr_t pageEnd = (reinterpret_cast<uintptr_t>(addr) + size + pageSize - 1) & ~(pageSize - 1);
        return mprotect(reinterpret_cast<void*>(pageStart), pageEnd - pageStart, PROT_READ | PROT_WRITE | PROT_EXEC) == 0;
    }

#if defined(__aarch64__)
    static size_t RelocateArm64Instruction(uint32_t insn, uintptr_t srcPc, uint8_t* outBuf) {
        uint32_t* out = reinterpret_cast<uint32_t*>(outBuf);
        
        // 1. ADRP Xd, #imm
        if ((insn & 0x9F000000) == 0x90000000) {
            uint32_t rd = insn & 0x1F;
            int64_t immlo = (insn >> 29) & 0x3;
            int64_t immhi = (insn >> 5) & 0x7FFFF;
            int64_t imm = (immhi << 2) | immlo;
            if (imm & 0x100000) imm |= ~0x1FFFFFULL;
            
            uint64_t pageBase = srcPc & ~0xFFFULL;
            uint64_t dest = pageBase + (imm << 12);
            
            out[0] = 0x58000040 | rd; // LDR Xd, [PC, #8]
            out[1] = 0x14000003;      // B +12
            *reinterpret_cast<uint64_t*>(&out[2]) = dest;
            return 16;
        }

        // 2. ADR Xd, #imm
        if ((insn & 0x9F000000) == 0x10000000) {
            uint32_t rd = insn & 0x1F;
            int64_t immlo = (insn >> 29) & 0x3;
            int64_t immhi = (insn >> 5) & 0x7FFFF;
            int64_t imm = (immhi << 2) | immlo;
            if (imm & 0x100000) imm |= ~0x1FFFFFULL;
            
            uint64_t dest = srcPc + imm;
            out[0] = 0x58000040 | rd; // LDR Xd, [PC, #8]
            out[1] = 0x14000003;      // B +12
            *reinterpret_cast<uint64_t*>(&out[2]) = dest;
            return 16;
        }

        // 3. LDR literal
        if ((insn & 0x3B000000) == 0x18000000) {
            uint32_t rd = insn & 0x1F;
            int64_t imm = (insn >> 5) & 0x7FFFF;
            if (imm & 0x40000) imm |= ~0x7FFFFULL;
            uint64_t targetAddr = srcPc + (imm * 4);
            
            out[0] = 0x58000060 | 16; // LDR X16, [PC, #12]
            out[1] = (insn & 0x40000000) ? (0xF9400200 | rd) : (0xB9400200 | rd); // LDR Xd, [X16]
            out[2] = 0x14000003;      // B +12
            *reinterpret_cast<uint64_t*>(&out[3]) = targetAddr;
            return 20;
        }

        // Default standard instruction
        out[0] = insn;
        return 4;
    }
#endif

    bool HookEngine::Hook(void* target, void* replace, void** origin) {
        if (!target || !replace) return false;

#if defined(__aarch64__)
        constexpr size_t HookSize = 16;
        
        HookRecord record;
        record.Target = target;
        record.OrigSize = HookSize;
        memcpy(record.OrigBytes, target, HookSize);

        // Allocate executable trampoline
        void* trampoline = mmap(nullptr, 128, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        if (trampoline == MAP_FAILED) {
            return false;
        }

        uint8_t* trampBytes = static_cast<uint8_t*>(trampoline);
        size_t trampOffset = 0;

        // Relocate original 16 bytes (4 instructions)
        uint32_t* origInsns = static_cast<uint32_t*>(target);
        for (int i = 0; i < 4; ++i) {
            uintptr_t srcPc = reinterpret_cast<uintptr_t>(target) + (i * 4);
            size_t written = RelocateArm64Instruction(origInsns[i], srcPc, trampBytes + trampOffset);
            trampOffset += written;
        }

        // Jump back to target + 16
        uint32_t trampJump[2] = { 0x58000050, 0xD61F0200 };
        uint64_t returnAddr = reinterpret_cast<uint64_t>(target) + HookSize;
        memcpy(trampBytes + trampOffset, trampJump, sizeof(trampJump));
        memcpy(trampBytes + trampOffset + 8, &returnAddr, sizeof(uint64_t));
        trampOffset += 16;

        __builtin___clear_cache(reinterpret_cast<char*>(trampBytes), reinterpret_cast<char*>(trampBytes + trampOffset));

        if (origin) {
            *origin = trampoline;
        }

        if (!SetMemoryWritable(target, HookSize)) {
            munmap(trampoline, 128);
            return false;
        }

        // Write jump in target
        uint32_t hookCode[2] = { 0x58000050, 0xD61F0200 };
        uint64_t targetDest = reinterpret_cast<uint64_t>(replace);
        
        memcpy(target, hookCode, sizeof(hookCode));
        memcpy(static_cast<uint8_t*>(target) + 8, &targetDest, sizeof(uint64_t));

        __builtin___clear_cache(reinterpret_cast<char*>(target), reinterpret_cast<char*>(target) + HookSize);

        record.Trampoline = trampoline;
        Hooks.push_back(record);
        return true;

#elif defined(__arm__)
        constexpr size_t HookSize = 8;

        HookRecord record;
        record.Target = target;
        record.OrigSize = HookSize;
        memcpy(record.OrigBytes, target, HookSize);

        void* trampoline = mmap(nullptr, 64, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
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
            munmap(trampoline, 64);
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
                    munmap(it->Trampoline, 128);
                }
                Hooks.erase(it);
                return true;
            }
        }
        return false;
    }
}
