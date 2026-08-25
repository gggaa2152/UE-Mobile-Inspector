#include "ptrace_helper.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <dlfcn.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <elf.h>
#include <android/log.h>

#define LOG_TAG "UE-Injector"
#define LOGI(...) do { printf("[*] " __VA_ARGS__); printf("\n"); fflush(stdout); } while(0)
#define LOGE(...) do { printf("[-] " __VA_ARGS__); printf("\n"); fflush(stdout); } while(0)
#define LOGS(...) do { printf("[+] " __VA_ARGS__); printf("\n"); fflush(stdout); } while(0)

#ifndef NT_PRSTATUS
#define NT_PRSTATUS 1
#endif

namespace Injector {

    bool PtraceAttach(pid_t pid) {
        if (ptrace(PTRACE_ATTACH, pid, nullptr, nullptr) < 0) {
            perror("ptrace(ATTACH)");
            return false;
        }
        int status = 0;
        waitpid(pid, &status, WUNTRACED);
        return true;
    }

    bool PtraceDetach(pid_t pid) {
        if (ptrace(PTRACE_DETACH, pid, nullptr, nullptr) < 0) {
            perror("ptrace(DETACH)");
            return false;
        }
        return true;
    }

    bool PtraceContinue(pid_t pid) {
        if (ptrace(PTRACE_CONT, pid, nullptr, nullptr) < 0) {
            perror("ptrace(CONT)");
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
        iov.iov_base = const_cast<Arm64Regs*>(&regs.regs);
        iov.iov_len = sizeof(regs.regs);
        if (ptrace(PTRACE_SETREGSET, pid, (void*)NT_PRSTATUS, &iov) < 0) {
            perror("ptrace(PTRACE_SETREGSET)");
            return false;
        }
        return true;
#elif defined(__arm__)
        if (ptrace(PTRACE_SETREGS, pid, nullptr, const_cast<Arm32Regs*>(&regs.regs)) < 0) {
            perror("ptrace(PTRACE_SETREGS)");
            return false;
        }
        return true;
#endif
    }

    bool PtraceReadBytes(pid_t pid, uintptr_t addr, void* dest, size_t size) {
        size_t bytesRead = 0;
        long* destPtr = reinterpret_cast<long*>(dest);
        uintptr_t srcAddr = addr;

        while (bytesRead < size) {
            errno = 0;
            long val = ptrace(PTRACE_PEEKDATA, pid, (void*)srcAddr, nullptr);
            if (errno != 0) {
                // Fallback to /proc/[pid]/mem
                char memPath[64];
                snprintf(memPath, sizeof(memPath), "/proc/%d/mem", pid);
                int fd = open(memPath, O_RDONLY);
                if (fd >= 0) {
                    lseek64(fd, addr, SEEK_SET);
                    ssize_t r = read(fd, dest, size);
                    close(fd);
                    return r == (ssize_t)size;
                }
                return false;
            }
            size_t copySize = (size - bytesRead >= sizeof(long)) ? sizeof(long) : (size - bytesRead);
            memcpy(reinterpret_cast<uint8_t*>(dest) + bytesRead, &val, copySize);
            bytesRead += copySize;
            srcAddr += copySize;
        }
        return true;
    }

    bool PtraceWriteBytes(pid_t pid, uintptr_t addr, const void* src, size_t size) {
        size_t bytesWritten = 0;
        const long* srcPtr = reinterpret_cast<const long*>(src);
        uintptr_t destAddr = addr;

        while (bytesWritten < size) {
            long val = 0;
            size_t copySize = (size - bytesWritten >= sizeof(long)) ? sizeof(long) : (size - bytesWritten);
            if (copySize < sizeof(long)) {
                val = ptrace(PTRACE_PEEKDATA, pid, (void*)destAddr, nullptr);
            }
            memcpy(&val, reinterpret_cast<const uint8_t*>(src) + bytesWritten, copySize);

            if (ptrace(PTRACE_POKEDATA, pid, (void*)destAddr, (void*)val) < 0) {
                // Fallback to /proc/[pid]/mem
                char memPath[64];
                snprintf(memPath, sizeof(memPath), "/proc/%d/mem", pid);
                int fd = open(memPath, O_WRONLY);
                if (fd >= 0) {
                    lseek64(fd, addr, SEEK_SET);
                    ssize_t w = write(fd, src, size);
                    close(fd);
                    return w == (ssize_t)size;
                }
                return false;
            }
            bytesWritten += copySize;
            destAddr += copySize;
        }
        return true;
    }

    uintptr_t GetRemoteModuleBase(pid_t pid, const char* moduleName) {
        char mapsPath[64];
        if (pid < 0) {
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
                baseAddr = strtoul(line, nullptr, 16);
                break;
            }
        }
        fclose(fp);
        return baseAddr;
    }

    uintptr_t GetRemoteFuncAddress(pid_t pid, const char* moduleName, void* localFuncAddr) {
        uintptr_t localModuleBase = GetRemoteModuleBase(-1, moduleName);
        uintptr_t remoteModuleBase = GetRemoteModuleBase(pid, moduleName);

        if (!localModuleBase || !remoteModuleBase) {
            return 0;
        }

        uintptr_t offset = reinterpret_cast<uintptr_t>(localFuncAddr) - localModuleBase;
        return remoteModuleBase + offset;
    }

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
            if (!fp) continue;

            char cmdline[256] = {0};
            fread(cmdline, 1, sizeof(cmdline) - 1, fp);
            fclose(fp);

            if (strcmp(cmdline, processName.c_str()) == 0 || strstr(cmdline, processName.c_str()) != nullptr) {
                closedir(dir);
                return static_cast<pid_t>(pid);
            }
        }
        closedir(dir);
        return -1;
    }

    // Executes a function in remote process, returning to trapAddr (where BRK #0 instruction is located)
    uintptr_t PtraceCall(pid_t pid, uintptr_t funcAddr, uintptr_t* args, size_t argCount, uintptr_t trapAddr) {
        Regs regs, origRegs;
        if (!PtraceGetRegs(pid, origRegs)) return 0;
        regs = origRegs;

#if defined(__aarch64__)
        for (size_t i = 0; i < argCount && i < 8; ++i) {
            regs.regs.regs[i] = args[i];
        }

        // Set return address (LR / X30) to trapAddr
        regs.regs.regs[30] = trapAddr;
        regs.regs.pc = funcAddr;

        if (regs.regs.sp % 16 != 0) {
            regs.regs.sp -= (regs.regs.sp % 16);
        }

        if (!PtraceSetRegs(pid, regs)) return 0;
        if (!PtraceContinue(pid)) return 0;

        int status = 0;
        waitpid(pid, &status, WUNTRACED);

        while (true) {
            if (WIFSTOPPED(status)) {
                int sig = WSTOPSIG(status);
                if (sig == SIGTRAP || sig == SIGSEGV) {
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

        regs.regs.ARM_lr = trapAddr;
        regs.regs.ARM_pc = funcAddr;
        if (regs.regs.ARM_pc & 1) {
            regs.regs.ARM_pc &= ~1;
            regs.regs.ARM_cpsr |= 0x20;
        } else {
            regs.regs.ARM_cpsr &= ~0x20;
        }

        if (!PtraceSetRegs(pid, regs)) return 0;
        if (!PtraceContinue(pid)) return 0;

        int status = 0;
        waitpid(pid, &status, WUNTRACED);

        while (true) {
            if (WIFSTOPPED(status)) {
                int sig = WSTOPSIG(status);
                if (sig == SIGTRAP || sig == SIGSEGV) {
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

        // 2. Allocate memory buffer in target process (0x1000 bytes)
        // [0x000 - 0x01F]: Trap instructions (BRK #0 on arm64)
        // [0x020 - 0xFFF]: SO path string & arguments
        size_t allocSize = 0x1000;
        uintptr_t mmapArgs[6] = {
            0,
            allocSize,
            PROT_READ | PROT_WRITE | PROT_EXEC,
            MAP_ANONYMOUS | MAP_PRIVATE,
            (uintptr_t)-1,
            0
        };

        // For initial mmap, use remoteMmap itself as trap address or 0
        uintptr_t remoteBuf = PtraceCall(pid, remoteMmap, mmapArgs, 6, 0);
        if (!remoteBuf || remoteBuf == (uintptr_t)MAP_FAILED) {
            LOGE("Failed to allocate remote memory buffer via mmap");
            PtraceDetach(pid);
            return false;
        }
        LOGS("Allocated remote buffer at: 0x%lx", (unsigned long)remoteBuf);

        // 3. Write BRK #0 instruction at remoteBuf + 0
        uintptr_t trapAddr = remoteBuf;
#if defined(__aarch64__)
        uint32_t trapCode = 0xD4200000; // BRK #0 instruction
#else
        uint32_t trapCode = 0xE7F001F0; // UDF #0 on ARM32
#endif
        PtraceWriteBytes(pid, trapAddr, &trapCode, sizeof(trapCode));

        // 4. Write library path at remoteBuf + 0x40
        uintptr_t pathAddr = remoteBuf + 0x40;
        if (!PtraceWriteBytes(pid, pathAddr, libraryPath.c_str(), libraryPath.length() + 1)) {
            LOGE("Failed to write library path into remote memory");
            PtraceDetach(pid);
            return false;
        }

        // 5. Resolve remote dlopen & dlerror
        void* localDlopen = reinterpret_cast<void*>(dlopen);
        uintptr_t remoteDlopen = GetRemoteFuncAddress(pid, "libdl.so", localDlopen);
        if (!remoteDlopen) remoteDlopen = GetRemoteFuncAddress(pid, "libc.so", localDlopen);
        if (!remoteDlopen) remoteDlopen = GetRemoteFuncAddress(pid, "linker64", localDlopen);
        if (!remoteDlopen) remoteDlopen = GetRemoteFuncAddress(pid, "linker", localDlopen);

        void* localDlerror = reinterpret_cast<void*>(dlerror);
        uintptr_t remoteDlerror = GetRemoteFuncAddress(pid, "libdl.so", localDlerror);
        if (!remoteDlerror) remoteDlerror = GetRemoteFuncAddress(pid, "libc.so", localDlerror);
        if (!remoteDlerror) remoteDlerror = GetRemoteFuncAddress(pid, "linker64", localDlerror);
        if (!remoteDlerror) remoteDlerror = GetRemoteFuncAddress(pid, "linker", localDlerror);

        if (!remoteDlopen) {
            LOGE("Failed to resolve remote dlopen");
            PtraceDetach(pid);
            return false;
        }
        LOGI("Resolved remote dlopen: 0x%lx", (unsigned long)remoteDlopen);

        // 6. Call dlopen(path, RTLD_NOW | RTLD_GLOBAL, caller_address)
        // Pass remoteMmap as caller_address to bypass Android Linker Namespace checks!
        uintptr_t dlopenArgs[3] = {
            pathAddr,
            RTLD_NOW | RTLD_GLOBAL,
            remoteMmap
        };

        LOGI("Invoking dlopen(\"%s\") in target process...", libraryPath.c_str());
        uintptr_t remoteHandle = PtraceCall(pid, remoteDlopen, dlopenArgs, 3, trapAddr);

        if (remoteHandle == 0 && remoteDlerror) {
            uintptr_t errPtr = PtraceCall(pid, remoteDlerror, nullptr, 0, trapAddr);
            if (errPtr != 0) {
                char errBuf[256] = {0};
                PtraceReadBytes(pid, errPtr, errBuf, sizeof(errBuf) - 1);
                LOGE("Remote dlerror: %s", errBuf);
            }
        }

        PtraceDetach(pid);

        if (remoteHandle != 0) {
            LOGS(">>> SUCCESS: Library injected successfully! Remote handle: 0x%lx <<<", (unsigned long)remoteHandle);
            return true;
        } else {
            LOGE("Remote dlopen returned NULL.");
            return false;
        }
    }
}
