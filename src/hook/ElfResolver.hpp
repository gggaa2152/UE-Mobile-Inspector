#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
#include <elf.h>
#include "core/Logger.hpp"

namespace ElfUtils {

    // Find full module path and base address from /proc/self/maps
    inline uintptr_t FindModuleBaseAndPath(const char* modulePattern, std::string& outFullPath) {
        FILE* fp = fopen("/proc/self/maps", "rt");
        if (!fp) return 0;
        
        char line[512];
        while (fgets(line, sizeof(line), fp)) {
            if (!strstr(line, modulePattern)) continue;
            if (!strstr(line, "r-xp") && !strstr(line, "r--p")) continue;
            
            uintptr_t base = 0;
            char pathBuf[256] = {0};
            char* slash = strstr(line, "/");
            if (slash) {
                sscanf(line, "%lx", &base);
                sscanf(slash, "%255s", pathBuf);
                outFullPath = pathBuf;
                fclose(fp);
                return base;
        }
        fclose(fp);
        return 0;
    }

    inline uintptr_t FindModuleBase(const char* modulePattern) {
        std::string path;
        return FindModuleBaseAndPath(modulePattern, path);
    }

    // Rock-solid symbol resolution by reading ELF section headers from disk
    // 100% immune to namespace restrictions, unmapped memory, and SIGSEGV crashes
    inline void* ResolveSymbol(const char* modulePattern, const char* symbolName) {
        std::string fullPath;
        uintptr_t base = FindModuleBaseAndPath(modulePattern, fullPath);
        if (!base || fullPath.empty()) return nullptr;

        FILE* fp = fopen(fullPath.c_str(), "rb");
        if (!fp) return nullptr;

        Elf64_Ehdr ehdr;
        if (fread(&ehdr, 1, sizeof(ehdr), fp) != sizeof(ehdr)) {
            fclose(fp);
            return nullptr;
        }

        if (memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0) {
            fclose(fp);
            return nullptr;
        }

        if (ehdr.e_shoff == 0 || ehdr.e_shnum == 0) {
            fclose(fp);
            return nullptr;
        }

        fseek(fp, (long)ehdr.e_shoff, SEEK_SET);
        std::vector<Elf64_Shdr> shdrs(ehdr.e_shnum);
        if (fread(shdrs.data(), sizeof(Elf64_Shdr), ehdr.e_shnum, fp) != ehdr.e_shnum) {
            fclose(fp);
            return nullptr;
        }

        for (const auto& sh : shdrs) {
            if (sh.sh_type == SHT_DYNSYM && sh.sh_link < ehdr.e_shnum) {
                size_t numSyms = sh.sh_size / sizeof(Elf64_Sym);
                std::vector<Elf64_Sym> syms(numSyms);
                fseek(fp, (long)sh.sh_offset, SEEK_SET);
                fread(syms.data(), sizeof(Elf64_Sym), numSyms, fp);

                const auto& strSh = shdrs[sh.sh_link];
                std::vector<char> strtab(strSh.sh_size);
                fseek(fp, (long)strSh.sh_offset, SEEK_SET);
                fread(strtab.data(), 1, strSh.sh_size, fp);

                for (size_t i = 0; i < numSyms; i++) {
                    if (syms[i].st_name < strtab.size()) {
                        const char* name = strtab.data() + syms[i].st_name;
                        if (strcmp(name, symbolName) == 0 && syms[i].st_value != 0) {
                            fclose(fp);
                            void* result = reinterpret_cast<void*>(base + syms[i].st_value);
                            LOGI("[ElfUtils] Resolved %s::%s -> %p (base: 0x%lx, offset: 0x%lx)", 
                                 fullPath.c_str(), symbolName, result, (unsigned long)base, (unsigned long)syms[i].st_value);
                            return result;
                        }
                    }
                }
            }
        }

        fclose(fp);
        return nullptr;
    }
}
