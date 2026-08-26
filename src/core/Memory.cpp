#include "Memory.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <link.h>
#include <sys/uio.h>
#include <unistd.h>
#include <inttypes.h>

namespace Memory {

    uintptr_t GetModuleBase(const char* moduleName) {
        FILE* fp = fopen("/proc/self/maps", "rt");
        if (!fp) return 0;

        char line[512];
        uintptr_t address = 0;

        while (fgets(line, sizeof(line), fp)) {
            if (strstr(line, moduleName)) {
                unsigned long long addr = 0;
                if (sscanf(line, "%llx-", &addr) == 1) {
                    address = static_cast<uintptr_t>(addr);
                }
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
                unsigned long long segStart = 0, segEnd = 0;
                if (sscanf(line, "%llx-%llx", &segStart, &segEnd) == 2) {
                    if (!found) {
                        startAddr = static_cast<uintptr_t>(segStart);
                        found = true;
                    }
                    endAddr = static_cast<uintptr_t>(segEnd);
                }
            }
        }
        fclose(fp);
        return (endAddr > startAddr) ? (endAddr - startAddr) : 0;
    }

    std::vector<SegmentInfo> GetModuleSegments(const char* moduleName) {
        std::vector<SegmentInfo> segments;
        FILE* fp = fopen("/proc/self/maps", "rt");
        if (!fp) return segments;

        char line[512];
        while (fgets(line, sizeof(line), fp)) {
            if (!moduleName || strstr(line, moduleName)) {
                unsigned long long segStart = 0, segEnd = 0;
                char perms[8] = {0};
                if (sscanf(line, "%llx-%llx %7s", &segStart, &segEnd, perms) == 3) {
                    SegmentInfo seg;
                    seg.start = static_cast<uintptr_t>(segStart);
                    seg.end = static_cast<uintptr_t>(segEnd);
                    seg.isReadable = (perms[0] == 'r');
                    seg.isWritable = (perms[1] == 'w');
                    seg.isExecutable = (perms[2] == 'x');
                    segments.push_back(seg);
                }
            }
        }
        fclose(fp);
        return segments;
    }

    bool IsAddressInExecutable(uintptr_t addr, const char* moduleName) {
        if (addr < 0x10000) return false;
        static auto segments = GetModuleSegments(moduleName);
        if (segments.empty()) segments = GetModuleSegments(moduleName);
        for (const auto& seg : segments) {
            if (seg.isExecutable && addr >= seg.start && addr < seg.end) {
                return true;
            }
        }
        return false;
    }

    uintptr_t FindPattern(uintptr_t start, size_t length, const char* pattern, const char* mask) {
        size_t patternLen = strlen(mask);
        if (length < patternLen) return 0;

        for (size_t i = 0; i <= length - patternLen; ++i) {
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
        uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
        if (!ptr || addr < 0x10000 || addr > 0x7fffffffff) {
            return false;
        }

        uint8_t buf[1];
        struct iovec local = { buf, sizeof(buf) };
        struct iovec remote = { const_cast<void*>(ptr), sizeof(buf) };
        
        static pid_t myPid = getpid();
        return process_vm_readv(myPid, &local, 1, &remote, 1, 0) == 1;
    }
}
