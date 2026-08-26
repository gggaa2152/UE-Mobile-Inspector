#include "UECore.hpp"
#include "../config.hpp"
#include <algorithm>
#include <cstring>
#include <dlfcn.h>
#include <android/log.h>

#define LOG_TAG "UE-Mobile-Inspector"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace UE {

    // Chunked FUObjectArray layout for UE4.21+ / UE5
    struct FUObjectItem {
        UObject* Object;
        int32_t Flags;
        int32_t ClusterRootIndex;
        int32_t SerialNumber;
    };

    struct TUObjectArray {
        FUObjectItem** Objects;
        FUObjectItem* PreAllocatedObjects;
        int32_t MaxElements;
        int32_t NumElements;
        int32_t MaxChunks;
        int32_t NumChunks;
    };

    static inline uint8_t DecryptDeltaForceByte(uint8_t byte, uint32_t strLength) {
        uint8_t key = 0;
        switch (strLength % 9) {
            case 0: key = ((strLength & 0x1F) + strLength + 0x80) | 0x7F; break;
            case 1: key = ((strLength ^ 0xDF) + strLength + 0x80) | 0x7F; break;
            case 2: key = ((strLength | 0xCF) + strLength + 128) | 0x7F; break;
            case 3: key = (33 * strLength + 128) | 0x7F; break;
            case 4: key = (strLength + (strLength >> 2) + 0x80) | 0x7F; break;
            case 5: key = (3 * strLength + 133) | 0x7F; break;
            case 6: key = (((4 * strLength) | 5) + strLength + 128) | 0x7F; break;
            case 7: key = (((strLength >> 4) | 7) + strLength + 128) | 0x7F; break;
            case 8: key = ((strLength ^ 0xC) + strLength + 0x80) | 0x7F; break;
            default: key = ((strLength ^ 0x40) + strLength + 128) | 0x7F; break;
        }
        return byte ^ key;
    }

    static std::string ExtractDecryptedFName(const uint8_t* entryPtr) {
        if (!Memory::IsValidPtr(entryPtr)) return "";
        uint16_t header = *reinterpret_cast<const uint16_t*>(entryPtr);
        uint32_t len = header >> 6;
        if (len == 0 || len >= 256) len = header >> 1;
        if (len == 0 || len >= 256) return "";

        const uint8_t* rawStr = entryPtr + 2;
        if (!Memory::IsValidPtr(rawStr)) return "";

        // 1. Raw printable check
        bool rawPrintable = true;
        for (uint32_t i = 0; i < len; ++i) {
            if (rawStr[i] < 32 || rawStr[i] > 126) {
                rawPrintable = false;
                break;
            }
        }
        if (rawPrintable) {
            return std::string(reinterpret_cast<const char*>(rawStr), len);
        }

        // 2. Delta Force XOR Decryption check (DreamFekk / dump-7 algorithm)
        std::string decrypted(len, '\0');
        for (uint32_t i = 0; i < len; ++i) {
            decrypted[i] = static_cast<char>(DecryptDeltaForceByte(rawStr[i], len));
        }

        bool decPrintable = true;
        for (uint32_t i = 0; i < len; ++i) {
            unsigned char c = static_cast<unsigned char>(decrypted[i]);
            if (c < 32 || c > 126) {
                decPrintable = false;
                break;
            }
        }
        if (decPrintable) {
            return decrypted;
        }

        return "";
    }

    std::string FName::ToString() const {
        uintptr_t gnames = CoreManager::Get().GetGNamesAddress();
        if (!gnames) return "None";

        // Try both standard 16-bit indexing and Delta Force 18-bit indexing
        struct IndexShift { uint32_t block; uint32_t offset; };
        IndexShift shifts[2] = {
            { static_cast<uint32_t>(ComparisonIndex >> 16), static_cast<uint32_t>(ComparisonIndex & 0xFFFF) },
            { static_cast<uint32_t>(ComparisonIndex >> 18), static_cast<uint32_t>(ComparisonIndex & 0x3FFFF) }
        };

        for (const auto& shift : shifts) {
            if (shift.block >= 1024) continue;

            for (int blockOffset : {0x38, 0x10, 0x08, 0x00, 0x18}) {
                uintptr_t* blocks = reinterpret_cast<uintptr_t*>(gnames + blockOffset);
                if (!Memory::IsValidPtr(blocks) || !Memory::IsValidPtr(&blocks[shift.block])) {
                    continue;
                }

                uintptr_t blockPtr = blocks[shift.block];
                if (!Memory::IsValidPtr(reinterpret_cast<void*>(blockPtr))) {
                    continue;
                }

                for (int stride : {2, 1, 4}) {
                    uintptr_t entry = blockPtr + shift.offset * stride;
                    if (!Memory::IsValidPtr(reinterpret_cast<void*>(entry))) {
                        continue;
                    }

                    std::string res = ExtractDecryptedFName(reinterpret_cast<const uint8_t*>(entry));
                    if (!res.empty()) {
                        if (Number > 0) {
                            res += "_" + std::to_string(Number - 1);
                        }
                        return res;
                    }
                }
            }
        }

        return "Name_" + std::to_string(ComparisonIndex);
    }

    std::string FField::GetName() const {
        return NamePrivate.ToString();
    }

    std::string FProperty::GetTypeName() const {
        if (!ClassPrivate) return "Unknown";
        // FFieldClass has FName Name
        FName* namePtr = reinterpret_cast<FName*>(reinterpret_cast<uint8_t*>(ClassPrivate) + 0x8);
        if (Memory::IsValidPtr(namePtr)) {
            return namePtr->ToString();
        }
        return "Property";
    }

    std::string UObject::GetName() const {
        if (!Memory::IsValidPtr(this)) return "None";
        return NamePrivate.ToString();
    }

    std::string UObject::GetFullName() const {
        if (!Memory::IsValidPtr(this)) return "None";
        std::string name = GetName();
        std::string className = ClassPrivate ? ClassPrivate->GetName() : "None";
        
        std::string outerChain = "";
        for (UObject* outer = OuterPrivate; outer; outer = outer->OuterPrivate) {
            if (!Memory::IsValidPtr(outer)) break;
            outerChain = outer->GetName() + "." + outerChain;
        }
        return className + " " + outerChain + name;
    }

    bool UObject::IsA(UClass* SomeBaseClass) const {
        if (!Memory::IsValidPtr(this) || !ClassPrivate || !SomeBaseClass) return false;
        for (UStruct* temp = ClassPrivate; temp; temp = temp->SuperStruct) {
            if (temp == SomeBaseClass) return true;
        }
        return false;
    }

    void UObject::ProcessEvent(UFunction* Function, void* Parms) {
        if (!Memory::IsValidPtr(this) || !Memory::IsValidPtr(Function)) return;
        
        using ProcessEventFn = void (*)(UObject*, UFunction*, void*);
        uintptr_t peAddr = CoreManager::Get().GetProcessEventAddress();
        if (peAddr) {
            reinterpret_cast<ProcessEventFn>(peAddr)(this, Function, Parms);
        } else if (VTable) {
            // Index 66 or 67 in VTable is standard ProcessEvent
            void** vtbl = reinterpret_cast<void**>(VTable);
            if (Memory::IsValidPtr(vtbl) && Memory::IsValidPtr(vtbl[67])) {
                reinterpret_cast<ProcessEventFn>(vtbl[67])(this, Function, Parms);
            }
        }
    }

    std::vector<FProperty*> UStruct::GetProperties() const {
        std::vector<FProperty*> properties;
        if (!Memory::IsValidPtr(this)) return properties;

        // Traverse ChildProperties (UE4.25+)
        for (FField* field = ChildProperties; field; field = field->Next) {
            if (!Memory::IsValidPtr(field)) break;
            properties.push_back(reinterpret_cast<FProperty*>(field));
        }

        // Include Super Struct properties if enabled
        if (Config::bIncludeSuperProperties && SuperStruct) {
            auto superProps = SuperStruct->GetProperties();
            properties.insert(properties.begin(), superProps.begin(), superProps.end());
        }

        return properties;
    }

    std::vector<UFunction*> UStruct::GetFunctions() const {
        std::vector<UFunction*> functions;
        if (!Memory::IsValidPtr(this)) return functions;

        for (UField* field = Children; field; field = field->Next) {
            if (!Memory::IsValidPtr(field)) break;
            // Check if field is a UFunction
            if (field->ClassPrivate && field->ClassPrivate->GetName() == "Function") {
                functions.push_back(reinterpret_cast<UFunction*>(field));
            }
        }

        if (Config::bIncludeSuperProperties && SuperStruct) {
            auto superFuncs = SuperStruct->GetFunctions();
            functions.insert(functions.begin(), superFuncs.begin(), superFuncs.end());
        }

        return functions;
    }

    bool CoreManager::Initialize() {
        if (bInitialized) return true;
        LOGI("Initializing UE Core Reflection Engine...");

        if (!ResolveOffsets()) {
            LOGE("Failed to resolve GNames or GUObjectArray offsets.");
            return false;
        }

        bInitialized = true;
        LOGI("UE Core initialized successfully! Objects Count: %d", GetObjectCount());
        return true;
    }

    static int g_ItemStride = 24;
    static int g_NumElementsOffset = 0x14;
    static int g_ObjectsOffset = 0x00;
    static int g_FNameBlockOffset = 0x38;

    static uintptr_t ScanGNamesHeuristic(const char* moduleName) {
        LOGI("[AutoScanner] Initiating Heuristic Scan for GNames / FNamePool...");
        static const char* kKnownKeywords[] = {
            "None", "ByteProperty", "IntProperty", "BoolProperty",
            "FloatProperty", "ObjectProperty", "NameProperty",
            "StructProperty", "ArrayProperty", "Object", "Class", "Function",
            "Actor", "PlayerController", "Character", "Pawn", "Engine", "World"
        };

        uintptr_t ueBase = Memory::GetModuleBase(moduleName);
        
        // 1. Fast Delta Force CN RVA range scan (Page-aligned, 50ms)
        if (ueBase) {
            uintptr_t searchStart = ueBase + 0x10000000;
            uintptr_t searchEnd = ueBase + 0x30000000;
            for (uintptr_t addr = searchStart; addr < searchEnd; addr += 0x1000) {
                if (!Memory::IsValidPtr(reinterpret_cast<void*>(addr))) continue;
                for (int blockOffset : {0x38, 0x10, 0x00, 0x08, 0x18, 0x20}) {
                    uintptr_t* blocks = reinterpret_cast<uintptr_t*>(addr + blockOffset);
                    if (!Memory::IsValidPtr(blocks) || !Memory::IsValidPtr(&blocks[0])) continue;
                    uintptr_t block0 = blocks[0];
                    if (!Memory::IsValidPtr(reinterpret_cast<void*>(block0))) continue;
                    
                    for (int testIdx : {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 16, 20, 24, 32, 50, 64, 100}) {
                        uintptr_t entry = block0 + testIdx * 2;
                        if (!Memory::IsValidPtr(reinterpret_cast<void*>(entry))) continue;
                        std::string parsed = ExtractDecryptedFName(reinterpret_cast<const uint8_t*>(entry));
                        if (!parsed.empty()) {
                            for (const char* kw : kKnownKeywords) {
                                if (parsed == kw) {
                                    g_FNameBlockOffset = blockOffset;
                                    LOGI("[AutoScanner] >>> Fast DeltaForce Range HIT: Discovered GNames at 0x%lx (RVA: 0x%lx, blockOffset: 0x%x, keyword: '%s') <<<",
                                         addr, addr - ueBase, blockOffset, parsed.c_str());
                                    return addr;
                                }
                            }
                        }
                    }
                }
            }
        }

        // 2. Full module segments fallback scan
        auto segments = Memory::GetModuleSegments(moduleName);
        for (const auto& seg : segments) {
            if (!seg.isReadable || !seg.isWritable) continue;
            for (uintptr_t addr = seg.start; addr + 0x40 < seg.end; addr += 8) {
                for (int blockOffset : {0x38, 0x10, 0x08, 0x00, 0x18, 0x20}) {
                    uintptr_t* blocks = reinterpret_cast<uintptr_t*>(addr + blockOffset);
                    if (!Memory::IsValidPtr(blocks) || !Memory::IsValidPtr(&blocks[0])) continue;
                    uintptr_t block0 = blocks[0];
                    if (!Memory::IsValidPtr(reinterpret_cast<void*>(block0))) continue;
                    for (int testIdx : {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 16, 20, 32, 50}) {
                        uintptr_t entry = block0 + testIdx * 2;
                        if (!Memory::IsValidPtr(reinterpret_cast<void*>(entry))) continue;
                        std::string parsed = ExtractDecryptedFName(reinterpret_cast<const uint8_t*>(entry));
                        if (!parsed.empty()) {
                            for (const char* kw : kKnownKeywords) {
                                if (parsed == kw) {
                                    g_FNameBlockOffset = blockOffset;
                                    LOGI("[AutoScanner] >>> Segments Scan HIT: Discovered GNames at 0x%lx (blockOffset: 0x%x, keyword: '%s') <<<",
                                         addr, blockOffset, parsed.c_str());
                                    return addr;
                                }
                            }
                        }
                    }
                }
            }
        }
        LOGI("[AutoScanner] Heuristic GNames scan did not find a match, using fallback.");
        return 0;
    }

    static uintptr_t ScanGUObjectArrayHeuristic(const char* moduleName, bool& outHasGCHeader) {
        LOGI("[AutoScanner] Initiating Universal Heuristic Scan for GUObjectArray...");
        auto segments = Memory::GetModuleSegments(moduleName);
        
        for (const auto& seg : segments) {
            if (!seg.isReadable || !seg.isWritable) continue;
            
            for (uintptr_t addr = seg.start; addr + 0x50 < seg.end; addr += 8) {
                for (bool hasHeader : {true, false}) {
                    uintptr_t baseObjAddr = hasHeader ? (addr + 0x10) : addr;
                    
                    // Test both Standard layout (+0x14 NumElements) and Compact layout (+0x0C NumElements)
                    for (int numOff : {0x14, 0x0C, 0x10, 0x18, 0x24}) {
                        int32_t numElements = *reinterpret_cast<int32_t*>(baseObjAddr + numOff);
                        if (numElements < 200 || numElements > 3000000) continue;

                        uintptr_t* objectsPtr = *reinterpret_cast<uintptr_t**>(baseObjAddr);
                        if (!Memory::IsValidPtr(objectsPtr) || !Memory::IsValidPtr(&objectsPtr[0])) continue;

                        uintptr_t chunk0 = objectsPtr[0];
                        if (!Memory::IsValidPtr(reinterpret_cast<void*>(chunk0))) continue;

                        // Test both 24-byte (0x18) and 16-byte (0x10) FUObjectItem strides
                        for (int stride : {24, 16}) {
                            int validObjectCount = 0;
                            for (int i = 0; i < std::min(numElements, 60); ++i) {
                                uintptr_t itemAddr = chunk0 + i * stride;
                                if (!Memory::IsValidPtr(reinterpret_cast<void*>(itemAddr))) break;
                                
                                UObject* obj = *reinterpret_cast<UObject**>(itemAddr);
                                if (Memory::IsValidPtr(obj)) {
                                    void** vtbl = reinterpret_cast<void**>(obj->VTable);
                                    if (Memory::IsValidPtr(vtbl) && Memory::IsAddressInExecutable(reinterpret_cast<uintptr_t>(vtbl), moduleName)) {
                                        validObjectCount++;
                                    }
                                }
                            }

                            if (validObjectCount >= 3) {
                                outHasGCHeader = hasHeader;
                                g_ItemStride = stride;
                                g_NumElementsOffset = numOff;
                                g_ObjectsOffset = 0;
                                LOGI("[AutoScanner] >>> SUCCESS: Discovered REAL GUObjectArray at 0x%lx (hasGCHeader: %d, numOff: 0x%x, stride: %d, NumElements: %d, ValidVTables: %d) <<<",
                                     addr, hasHeader ? 1 : 0, numOff, stride, numElements, validObjectCount);
                                return addr;
                            }
                        }
                    }
                }
            }
        }
        LOGI("[AutoScanner] Heuristic GUObjectArray scan did not find a match.");
        return 0;
    }

    bool CoreManager::ResolveOffsets() {
        uintptr_t ueBase = Memory::GetModuleBase(Config::UE_SO_NAME);
        if (!ueBase) {
            LOGE("Cannot find module %s", Config::UE_SO_NAME);
            return false;
        }

        // 1. Dynamic Heuristic Auto-Scanner
        GNamesAddr = ScanGNamesHeuristic(Config::UE_SO_NAME);
        GUObjectArrayAddr = ScanGUObjectArrayHeuristic(Config::UE_SO_NAME, bArrayHasGCHeader);

        // 2. Fallback to exported symbols
        if (!GNamesAddr) {
            void* handle = dlopen(Config::UE_SO_NAME, RTLD_NOLOAD);
            if (handle) {
                GNamesAddr = reinterpret_cast<uintptr_t>(dlsym(handle, "GNames"));
                if (!GNamesAddr) GNamesAddr = reinterpret_cast<uintptr_t>(dlsym(handle, "FNamePool"));
            }
        }
        if (!GUObjectArrayAddr) {
            void* handle = dlopen(Config::UE_SO_NAME, RTLD_NOLOAD);
            if (handle) {
                GUObjectArrayAddr = reinterpret_cast<uintptr_t>(dlsym(handle, "GUObjectArray"));
            }
        }

        // 3. Fallback to Config offsets
        if (!GNamesAddr) {
            GNamesAddr = ueBase + Config::GNamesOffset;
            LOGI("[AutoScanner] Fallback to Config GNames: 0x%lx", GNamesAddr);
        }
        if (!GUObjectArrayAddr) {
            GUObjectArrayAddr = ueBase + Config::GUObjectArrayOffset;
            LOGI("[AutoScanner] Fallback to Config GUObjectArray: 0x%lx", GUObjectArrayAddr);
        }

        LOGI("[AutoScanner] Resolved: GNames=0x%lx, GUObjectArray=0x%lx (hasGCHeader: %d)",
             GNamesAddr, GUObjectArrayAddr, bArrayHasGCHeader ? 1 : 0);
        return (GUObjectArrayAddr != 0);
    }

    void CoreManager::ForceApplyOffsets(uintptr_t gnamesOffset, uintptr_t guobjectOffset) {
        Config::GNamesOffset = gnamesOffset;
        Config::GUObjectArrayOffset = guobjectOffset;
        
        uintptr_t ueBase = Memory::GetModuleBase(Config::UE_SO_NAME);
        if (ueBase) {
            GNamesAddr = ueBase + gnamesOffset;
            GUObjectArrayAddr = ueBase + guobjectOffset;
            LOGI("Forced new offsets: GNames=0x%lx, GUObjectArray=0x%lx", GNamesAddr, GUObjectArrayAddr);
        }
    }

    int32_t CoreManager::GetObjectCount() const {
        if (!GUObjectArrayAddr) return 0;
        uintptr_t arrayAddr = bArrayHasGCHeader ? (GUObjectArrayAddr + 0x10) : GUObjectArrayAddr;
        if (!Memory::IsValidPtr(reinterpret_cast<void*>(arrayAddr + g_NumElementsOffset))) return 0;
        return *reinterpret_cast<int32_t*>(arrayAddr + g_NumElementsOffset);
    }

    UObject* CoreManager::GetObjectByIndex(int32_t Index) const {
        if (!GUObjectArrayAddr || Index < 0 || Index >= GetObjectCount()) return nullptr;
        
        int32_t ElementsPerChunk = 65536; // 64K objects per chunk
        int32_t ChunkIndex = Index / ElementsPerChunk;
        int32_t InChunkIndex = Index % ElementsPerChunk;

        uintptr_t arrayAddr = bArrayHasGCHeader ? (GUObjectArrayAddr + 0x10) : GUObjectArrayAddr;
        uintptr_t* objectsPtr = *reinterpret_cast<uintptr_t**>(arrayAddr);
        if (!Memory::IsValidPtr(objectsPtr) || !Memory::IsValidPtr(&objectsPtr[ChunkIndex])) {
            return nullptr;
        }

        uintptr_t chunk = objectsPtr[ChunkIndex];
        if (!Memory::IsValidPtr(reinterpret_cast<void*>(chunk))) return nullptr;

        uintptr_t itemAddr = chunk + InChunkIndex * g_ItemStride;
        if (!Memory::IsValidPtr(reinterpret_cast<void*>(itemAddr))) return nullptr;

        return *reinterpret_cast<UObject**>(itemAddr);
    }

    std::vector<UClass*> CoreManager::GetAllClasses() {
        std::vector<UClass*> classes;
        int32_t count = GetObjectCount();
        for (int32_t i = 0; i < count; ++i) {
            UObject* obj = GetObjectByIndex(i);
            if (obj && obj->ClassPrivate && obj->ClassPrivate->GetName() == "Class") {
                classes.push_back(reinterpret_cast<UClass*>(obj));
            }
        }
        return classes;
    }

    std::vector<UObject*> CoreManager::FindInstancesOfClass(UClass* TargetClass) {
        std::vector<UObject*> instances;
        if (!TargetClass) return instances;

        int32_t count = GetObjectCount();
        for (int32_t i = 0; i < count; ++i) {
            UObject* obj = GetObjectByIndex(i);
            if (obj && obj->IsA(TargetClass)) {
                instances.push_back(obj);
            }
        }
        return instances;
    }

    UObject* CoreManager::FindObject(const std::string& FullName) {
        int32_t count = GetObjectCount();
        for (int32_t i = 0; i < count; ++i) {
            UObject* obj = GetObjectByIndex(i);
            if (obj && obj->GetFullName() == FullName) {
                return obj;
            }
        }
        return nullptr;
    }

    UClass* CoreManager::FindClass(const std::string& ClassName) {
        int32_t count = GetObjectCount();
        for (int32_t i = 0; i < count; ++i) {
            UObject* obj = GetObjectByIndex(i);
            if (obj && obj->ClassPrivate && obj->ClassPrivate->GetName() == "Class") {
                if (obj->GetName() == ClassName || obj->GetName() == "Class " + ClassName) {
                    return reinterpret_cast<UClass*>(obj);
                }
            }
        }
        return nullptr;
    }
}
