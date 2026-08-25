#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <elf.h>

// Manual ELF symbol resolution from mapped memory
// Bypasses Android linker namespace restrictions by reading symbols directly
namespace ElfUtils {

    // Find module base address from /proc/self/maps
    inline uintptr_t FindModuleBase(const char* moduleName) {
        FILE* fp = fopen("/proc/self/maps", "rt");
        if (!fp) return 0;
        
        char line[512];
        while (fgets(line, sizeof(line), fp)) {
            if (!strstr(line, moduleName)) continue;
            if (!strstr(line, "r-xp") && !strstr(line, "r--p")) continue;
            
            uintptr_t base = 0;
            sscanf(line, "%lx", &base);
            fclose(fp);
            return base;
        }
        fclose(fp);
        return 0;
    }

    // Resolve a symbol from a loaded ELF module by parsing its in-memory ELF headers
    inline void* ResolveSymbol(const char* moduleName, const char* symbolName) {
        uintptr_t base = FindModuleBase(moduleName);
        if (!base) return nullptr;

        // Verify ELF magic
        Elf64_Ehdr* ehdr = reinterpret_cast<Elf64_Ehdr*>(base);
        if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0) return nullptr;

        // Find program headers
        Elf64_Phdr* phdr = reinterpret_cast<Elf64_Phdr*>(base + ehdr->e_phoff);
        
        // Find PT_DYNAMIC segment
        Elf64_Dyn* dynamic = nullptr;
        for (int i = 0; i < ehdr->e_phnum; i++) {
            if (phdr[i].p_type == PT_DYNAMIC) {
                dynamic = reinterpret_cast<Elf64_Dyn*>(base + phdr[i].p_vaddr);
                break;
            }
        }
        if (!dynamic) return nullptr;

        // Extract .dynsym, .dynstr, and hash info from dynamic section
        Elf64_Sym* dynsym = nullptr;
        const char* dynstr = nullptr;
        uint32_t nchain = 0;  // number of symbols (from DT_HASH)
        Elf64_Word* hashtab = nullptr;

        for (Elf64_Dyn* d = dynamic; d->d_tag != DT_NULL; d++) {
            switch (d->d_tag) {
                case DT_SYMTAB:
                    dynsym = reinterpret_cast<Elf64_Sym*>(base + d->d_un.d_ptr);
                    break;
                case DT_STRTAB:
                    dynstr = reinterpret_cast<const char*>(base + d->d_un.d_ptr);
                    break;
                case DT_HASH:
                    hashtab = reinterpret_cast<Elf64_Word*>(base + d->d_un.d_ptr);
                    nchain = hashtab[1]; // hash[0]=nbucket, hash[1]=nchain (total syms)
                    break;
            }
        }

        if (!dynsym || !dynstr) return nullptr;

        // If no DT_HASH, try scanning up to a reasonable limit
        if (nchain == 0) nchain = 4096;

        // Linear scan through symbol table
        for (uint32_t i = 0; i < nchain; i++) {
            if (dynsym[i].st_name == 0) continue;
            if (dynsym[i].st_value == 0) continue;
            const char* name = dynstr + dynsym[i].st_name;
            if (strcmp(name, symbolName) == 0) {
                return reinterpret_cast<void*>(base + dynsym[i].st_value);
            }
        }

        return nullptr;
    }
}
