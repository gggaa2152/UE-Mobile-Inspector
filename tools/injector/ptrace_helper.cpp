#include "ptrace_helper.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <dlfcn.h>
#include <elf.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <unistd.h>
#include <android/log.h>

#define LOG_TAG "UE-Injector"
#define LOGI(...) printf("[*] " __VA_ARGS__); printf("\n")
#define LOGE(...) printf("[-] " __VA_ARGS__); printf("\n")

namespace Injector {

    pid_t FindProcessId(const std::string& processName) {
        DIR* dir = opendir("/proc");
        if (!dir) return -1;

        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            int pid = atoi(entry->d_name);
            if (pid <= 0) continue;

            char cmdlinePath[64];
            snprintf(cmdlinePath, sizeof(cmdlinePath), "/proc/%d/cmdline", pid);

            FILE* fp = fopen(cmdlinePath, "rb");
            if (fp) {
                char cmdline[256] = {0};
                size_t readBytes = fread(cmdline, 1, sizeof(cmdline) - 1, fp);
                fclose(fp);

                if (readBytes > 0) {
                    if (strstr(cmdline, processName.c_str()) != nullptr) {
                        closedir(dir);
                        return static_cast<pid_t>(pid);
                    }
                }
            }
        }
        closedir(dir);
        return -1;
    }

    uintptr_t GetRemoteModuleBase(pid_t pid, const char* moduleName) {
        char mapsPath[64];
        if (pid == -1) {
            snprintf(mapsPath, sizeof(mapsPath), "/proc/self/maps");
        } else {
            snprintf(mapsPath, sizeof(mapsPath), "/proc/%d/maps", pid);
        }

        FILE* fp = fopen(mapsPath, "rt");
        if (!fp) return 0;

        char line[512];
        uintptr_t baseAddr = 0;

        while (fgets(line, sizeof(line), fp)) {
            if (strstr(line, moduleName)) {
                unsigned long long addr = 0;
                if (sscanf(line, "%llx-", &addr) == 1) {
                    baseAddr = static_cast<uintptr_t>(addr);
                    break;
                }
            }
        }
        fclose(fp);
        return baseAddr;
    }

    uintptr_t GetRemoteFuncAddress(pid_t pid, const char* moduleName, void* localFuncAddr) {
        uintptr_t localModuleBase = GetRemoteModuleBase(-1, moduleName);
        if (!localModuleBase) return 0;

        uintptr_t remoteModuleBase = GetRemoteModuleBase(pid, moduleName);
        if (!remoteModuleBase) return 0;

        uintptr_t offset = reinterpret_cast<uintptr_t>(localFuncAddr) - localModuleBase;
        return remoteModuleBase + offset;
    }

    bool PtraceAttach(pid_t pid) {
        if (ptrace(PTRACE_ATTACH, pid, nullptr, nullptr) < 0) {
            perror("ptrace(PTRACE_ATTACH)");
            return false;
        }

        int status = 0;
        waitpid(pid, &status, WUNTRACED);
        return true;
    }

    bool PtraceDetach(pid_t pid) {
        if (ptrace(PTRACE_DETACH, pid, nullptr, nullptr) < 0) {
            perror("ptrace(PTRACE_DETACH)");
            return false;
        }
        return true;
    }

    bool PtraceGetRegs(pid_t pid, Regs& regs) {
#if defined(__aarch64__)
        struct iovec iov;
        iov.iov_base = &regs.regs;
        iov.iov_len = sizeof(regs.regs);
        if (ptrace(PTRACE_GETREGSET, pid, (void*)NT_PRSTATUS, &iov) < 0) {
            perror("ptrace(PTRACE_GETREGSET)");
            return false;
        }
        return true;
#elif defined(__arm__)
        if (ptrace(PTRACE_GETREGS, pid, nullptr, &regs.regs) < 0) {
            perror("ptrace(PTRACE_GETREGS)");
            return false;
        }
        return true;
#endif
    }

    bool PtraceSetRegs(pid_t pid, const Regs& regs) {
#if defined(__aarch64__)
        struct iovec iov;
        iov.iov_base = (void*)&regs.regs;
        iov.iov_len = sizeof(regs.regs);
        if (ptrace(PTRACE_SETREGSET, pid, (void*)NT_PRSTATUS, &iov) < 0) {
            perror("ptrace(PTRACE_SETREGSET)");
            return false;
        }
        return true;
#elif defined(__arm__)
        if (ptrace(PTRACE_SETREGS, pid, nullptr, (void*)&regs.regs) < 0) {
            perror("ptrace(PTRACE_SETREGS)");
            return false;
        }
        return true;
#endif
    }

    bool PtraceContinue(pid_t pid) {
        if (ptrace(PTRACE_CONT, pid, nullptr, nullptr) < 0) {
            perror("ptrace(PTRACE_CONT)");
            return false;
        }
        return true;
    }

    bool PtraceWriteBytes(pid_t pid, uintptr_t destAddr, const void* data, size_t length) {
        size_t wordCount = (length + sizeof(long) - 1) / sizeof(long);
        const long* srcWords = reinterpret_cast<const long*>(data);
        long* destWords = reinterpret_cast<long*>(destAddr);

        for (size_t i = 0; i < wordCount; ++i) {
            if (ptrace(PTRACE_POKETEXT, pid, destWords + i, reinterpret_cast<void*>(srcWords[i])) < 0) {
                return false;
            }
        }
        return true;
    }

    bool PtraceReadBytes(pid_t pid, uintptr_t srcAddr, void* buffer, size_t length) {
        size_t wordCount = (length + sizeof(long) - 1) / sizeof(long);
        long* destWords = reinterpret_cast<long*>(buffer);
        const long* srcWords = reinterpret_cast<const long*>(srcAddr);

        for (size_t i = 0; i < wordCount; ++i) {
            long val = ptrace(PTRACE_PEEKTEXT, pid, srcWords + i, nullptr);
            destWords[i] = val;
        }
        return true;
    }

    uintptr_t PtraceCall(pid_t pid, uintptr_t funcAddr, uintptr_t* args, size_t argCount) {
        Regs regs, origRegs;
        if (!PtraceGetRegs(pid, origRegs)) return 0;
        regs = origRegs;

#if defined(__aarch64__)
        for (size_t i = 0; i < argCount && i < 8; ++i) {
            regs.regs.regs[i] = args[i];
        }

        // Set return address to 0 to trigger SIGSEGV upon return
        regs.regs.regs[30] = 0; // LR
        regs.regs.pc = funcAddr;

        if (regs.regs.sp % 16 != 0) {
            regs.regs.sp -= (regs.regs.sp % 16);
        }

        if (!PtraceSetRegs(pid, regs)) return 0;
        if (!PtraceContinue(pid)) return 0;

        int status = 0;
        waitpid(pid, &status, WUNTRACED);

        while (status != 0xb7f) {
            if (WIFSTOPPED(status)) {
                if (WSTOPSIG(status) == SIGSEGV || WSTOPSIG(status) == SIGTRAP) {
                    break;
                }
            }
            if (!PtraceContinue(pid)) break;
            waitpid(pid, &status, WUNTRACED);
        }

        Regs resultRegs;
        PtraceGetRegs(pid, resultRegs);
        PtraceSetRegs(pid, origRegs);

        return resultRegs.regs.regs[0]; // Return value in X0

#elif defined(__arm__)
        for (size_t i = 0; i < argCount && i < 4; ++i) {
            regs.regs.uregs[i] = args[i];
        }

        if (argCount > 4) {
            regs.regs.ARM_sp -= (argCount - 4) * sizeof(uintptr_t);
            PtraceWriteBytes(pid, regs.regs.ARM_sp, &args[4], (argCount - 4) * sizeof(uintptr_t));
        }

        regs.regs.ARM_lr = 0;
        regs.regs.ARM_pc = funcAddr;
        if (regs.regs.ARM_pc & 1) {
            regs.regs.ARM_pc &= ~1;
            regs.regs.ARM_cpsr |= 0x20; // Thumb mode
        } else {
            regs.regs.ARM_cpsr &= ~0x20; // ARM mode
        }

        if (!PtraceSetRegs(pid, regs)) return 0;
        if (!PtraceContinue(pid)) return 0;

        int status = 0;
        waitpid(pid, &status, WUNTRACED);

        while (status != 0xb7f) {
            if (WIFSTOPPED(status)) {
                if (WSTOPSIG(status) == SIGSEGV || WSTOPSIG(status) == SIGTRAP) {
                    break;
                }
            }
            if (!PtraceContinue(pid)) break;
            waitpid(pid, &status, WUNTRACED);
        }

        Regs resultRegs;
        PtraceGetRegs(pid, resultRegs);
        PtraceSetRegs(pid, origRegs);

        return resultRegs.regs.ARM_r0;
#endif
    }

    bool InjectLibrary(pid_t pid, const std::string& libraryPath) {
        LOGI("Attaching to target process (PID: %d)...", pid);
        if (!PtraceAttach(pid)) {
            LOGE("Failed to attach to process %d. Please make sure injector runs as root.", pid);
            return false;
        }

        // 1. Resolve remote mmap
        void* localMmap = reinterpret_cast<void*>(mmap);
        uintptr_t remoteMmap = GetRemoteFuncAddress(pid, "libc.so", localMmap);
        if (!remoteMmap) {
            LOGE("Failed to resolve remote mmap in libc.so");
            PtraceDetach(pid);
            return false;
        }
        LOGI("Resolved remote mmap: 0x%lx", (unsigned long)remoteMmap);

        // 2. Allocate memory in remote process for SO path string
        size_t allocSize = 0x1000;
        uintptr_t mmapArgs[6] = {
            0,
            allocSize,
            PROT_READ | PROT_WRITE | PROT_EXEC,
            MAP_ANONYMOUS | MAP_PRIVATE,
            (uintptr_t)-1,
            0
        };

        uintptr_t remoteBuf = PtraceCall(pid, remoteMmap, mmapArgs, 6);
        if (!remoteBuf || remoteBuf == (uintptr_t)MAP_FAILED) {
            LOGE("Failed to allocate remote buffer via mmap");
            PtraceDetach(pid);
            return false;
        }
        LOGI("Allocated remote buffer at: 0x%lx", (unsigned long)remoteBuf);

        // 3. Write library path to remote buffer
        if (!PtraceWriteBytes(pid, remoteBuf, libraryPath.c_str(), libraryPath.length() + 1)) {
            LOGE("Failed to write library path into remote memory");
            PtraceDetach(pid);
            return false;
        }

        // 4. Resolve remote dlopen (check libdl.so / libc.so / linker)
        void* localDlopen = reinterpret_cast<void*>(dlopen);
        uintptr_t remoteDlopen = GetRemoteFuncAddress(pid, "libdl.so", localDlopen);
        if (!remoteDlopen) {
            remoteDlopen = GetRemoteFuncAddress(pid, "libc.so", localDlopen);
        }
        if (!remoteDlopen) {
            remoteDlopen = GetRemoteFuncAddress(pid, "linker64", localDlopen);
        }
        if (!remoteDlopen) {
            remoteDlopen = GetRemoteFuncAddress(pid, "linker", localDlopen);
        }

        if (!remoteDlopen) {
            LOGE("Failed to resolve remote dlopen");
            PtraceDetach(pid);
            return false;
        }
        LOGI("Resolved remote dlopen: 0x%lx", (unsigned long)remoteDlopen);

        // 5. Call dlopen(path, RTLD_NOW | RTLD_GLOBAL)
        uintptr_t dlopenArgs[2] = {
            remoteBuf,
            RTLD_NOW | RTLD_GLOBAL
        };

        LOGI("Invoking dlopen(\"%s\") in remote process...", libraryPath.c_str());
        uintptr_t remoteHandle = PtraceCall(pid, remoteDlopen, dlopenArgs, 2);
        LOGI("Remote dlopen returned handle: 0x%lx", (unsigned long)remoteHandle);

        PtraceDetach(pid);

        if (remoteHandle != 0) {
            LOGI(">>> SUCCESS: Library injected successfully! <<<");
            return true;
        } else {
            LOGE("Remote dlopen returned NULL. Check SELinux or file permissions (/data/local/tmp/).");
            return false;
        }
    }
}
