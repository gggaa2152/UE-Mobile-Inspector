#include "Memory.hpp"
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <link.h>
#include <sys/uio.h>
#include <unistd.h>

namespace Memory {

    uintptr_t GetModuleBase(const char* moduleName) {
        FILE* fp = fopen("/proc/self/maps", "rt");
        if (!fp) return 0;

        char line[512];
        uintptr_t address = 0;

        while (fgets(line, sizeof(line), fp)) {
            if (strstr(line, moduleName)) {
                address = strtoul(line, nullptr, 16);
                break;
            }
        }
        fclose(fp);
        return address;
    }

    size_t GetModuleSize(const char* moduleName) {
        FILE* fp = fopen("/proc/self/maps", "rt");
        if (!fp) return 0;

        char line[512];
        uintptr_t startAddr = 0, endAddr = 0;
        bool found = false;

        while (fgets(line, sizeof(line), fp)) {
            if (strstr(line, moduleName)) {
                uintptr_t segStart = 0, segEnd = 0;
                if (sscanf(line, "%lx-%lx", &segStart, &segEnd) == 2) {
                    if (!found) {
                        startAddr = segStart;
                        found = true;
                    }
                    endAddr = segEnd;
                }
            }
        }
        fclose(fp);
        return (endAddr > startAddr) ? (endAddr - startAddr) : 0;
    }

    uintptr_t FindPattern(uintptr_t start, size_t length, const char* pattern, const char* mask) {
        size_t patternLen = strlen(mask);
        for (size_t i = 0; i < length - patternLen; ++i) {
            bool found = true;
            for (size_t j = 0; j < patternLen; ++j) {
                if (mask[j] != '?' && pattern[j] != *reinterpret_cast<const char*>(start + i + j)) {
                    found = false;
                    break;
                }
            }
            if (found) {
                return start + i;
            }
        }
        return 0;
    }

    uintptr_t FindPattern(const char* moduleName, const char* pattern, const char* mask) {
        uintptr_t base = GetModuleBase(moduleName);
        if (!base) return 0;
        size_t size = GetModuleSize(moduleName);
        if (!size) return 0;
        return FindPattern(base, size, pattern, mask);
    }

    bool IsValidPtr(const void* ptr) {
        if (!ptr || reinterpret_cast<uintptr_t>(ptr) < 0x10000) {
            return false;
        }

        // Test pointer readability using process_vm_readv / pipe check
        int pipefd[2];
        if (pipe(pipefd) < 0) {
            return true; // Fallback
        }

        ssize_t written = write(pipefd[1], ptr, 1);
        close(pipefd[0]);
        close(pipefd[1]);

        return written == 1;
    }
}
