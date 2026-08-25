#pragma once

#include <sys/types.h>
#include <sys/user.h>
#include <cstdint>
#include <string>

namespace Injector {

    // Structure to hold register states across architectures
    struct Regs {
#if defined(__aarch64__)
        struct user_pt_regs regs;
#elif defined(__arm__)
        struct pt_regs regs;
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
