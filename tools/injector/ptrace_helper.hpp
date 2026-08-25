#pragma once

#include <sys/types.h>
#include <sys/user.h>
#include <sys/ptrace.h>
#include <linux/ptrace.h>
#include <asm/ptrace.h>
#include <cstdint>
#include <string>

#if defined(__aarch64__)
    struct Arm64Regs {
        uint64_t regs[31];
        uint64_t sp;
        uint64_t pc;
        uint64_t pstate;
    };
#elif defined(__arm__)
    struct Arm32Regs {
        uint32_t uregs[18];
    };
#ifndef ARM_r0
#define ARM_r0 uregs[0]
#define ARM_sp uregs[13]
#define ARM_lr uregs[14]
#define ARM_pc uregs[15]
#define ARM_cpsr uregs[16]
#endif
#endif

    // Structure to hold register states across architectures
    struct Regs {
#if defined(__aarch64__)
        Arm64Regs regs;
#elif defined(__arm__)
        Arm32Regs regs;
#endif
    };

    // Find PID of a process by package or process name
    pid_t FindProcessId(const std::string& processName);

    // Get module base address in target process
    uintptr_t GetRemoteModuleBase(pid_t pid, const char* moduleName);

    // Calculate remote function address using local vs remote module base
    uintptr_t GetRemoteFuncAddress(pid_t pid, const char* moduleName, void* localFuncAddr);

    // PTrace operations
    bool PtraceAttach(pid_t pid);
    bool PtraceDetach(pid_t pid);
    bool PtraceGetRegs(pid_t pid, Regs& regs);
    bool PtraceSetRegs(pid_t pid, const Regs& regs);
    bool PtraceContinue(pid_t pid);

    // Memory write/read to remote process
    bool PtraceWriteBytes(pid_t pid, uintptr_t destAddr, const void* data, size_t length);
    bool PtraceReadBytes(pid_t pid, uintptr_t srcAddr, void* buffer, size_t length);

    // Remote function invocation
    uintptr_t PtraceCall(pid_t pid, uintptr_t funcAddr, uintptr_t* args, size_t argCount);

    // High-level Injection routine
    bool InjectLibrary(pid_t pid, const std::string& libraryPath);
}
