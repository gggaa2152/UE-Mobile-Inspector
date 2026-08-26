#include "UECore.hpp"
#include "../config.hpp"
#include <algorithm>
#include <cstring>
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

    std::string FName::ToString() const {
        uintptr_t gnames = CoreManager::Get().GetGNamesAddress();
        if (!gnames) return "None";

        uint32_t block = ComparisonIndex >> 16;
        uint32_t offset = ComparisonIndex & 65535;

        for (int blockOffset : {0x10, 0x08, 0x00, 0x18}) {
            uintptr_t* blocks = reinterpret_cast<uintptr_t*>(gnames + blockOffset);
            if (!Memory::IsValidPtr(blocks) || !Memory::IsValidPtr(reinterpret_cast<void*>(blocks[block]))) {
                continue;
            }

            uintptr_t entry = blocks[block] + offset * 2;
            if (!Memory::IsValidPtr(reinterpret_cast<void*>(entry))) {
                continue;
            }

            uint16_t header = *reinterpret_cast<uint16_t*>(entry);
            uint32_t len = header >> 6;
            if (len == 0 || len >= 1024) len = header >> 1; // Fallback for UE5 / alternate layout

            if (len > 0 && len < 1024) {
                char buf[1024] = {0};
                memcpy(buf, reinterpret_cast<const void*>(entry + 2), len);
                buf[len] = '\0';
                
                // Check if result contains printable characters
                bool isPrintable = true;
                for (size_t i = 0; i < len; ++i) {
                    if (buf[i] < 32 || buf[i] > 126) {
                        isPrintable = false;
                        break;
                    }
                }

                if (isPrintable) {
                    std::string result(buf);
                    if (Number > 0) {
                        result += "_" + std::to_string(Number - 1);
                    }
                    return result;
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

    static uintptr_t ScanGNamesHeuristic(const char* moduleName) {
        LOGI("[AutoScanner] Initiating Heuristic Scan for GNames / FNamePool...");
        auto segments = Memory::GetModuleSegments(moduleName);
        
        for (const auto& seg : segments) {
            if (!seg.isReadable) continue;
            
            for (uintptr_t addr = seg.start; addr + 0x40 < seg.end; addr += 8) {
                for (int blockOffset : {0x10, 0x08, 0x00, 0x18}) {
                    uintptr_t* blocks = reinterpret_cast<uintptr_t*>(addr + blockOffset);
                    if (!Memory::IsValidPtr(blocks)) continue;
                    
                    uintptr_t block0 = blocks[0];
                    if (!Memory::IsValidPtr(reinterpret_cast<void*>(block0))) continue;
                    
                    for (int entryOffset : {0, 2, 4}) {
                        uintptr_t entry = block0 + entryOffset;
                        if (!Memory::IsValidPtr(reinterpret_cast<void*>(entry))) continue;
                        
                        const char* str = reinterpret_cast<const char*>(entry + 2);
                        if (Memory::IsValidPtr(str) && (strncmp(str, "None", 4) == 0 || strncmp(str, "ByteProperty", 12) == 0)) {
                            LOGI("[AutoScanner] >>> SUCCESS: Discovered GNames/FNamePool at 0x%lx (blockOffset: 0x%x, str: %s) <<<",
                                 addr, blockOffset, str);
                            return addr;
                        }
                        
                        const char* strDirect = reinterpret_cast<const char*>(entry);
                        if (Memory::IsValidPtr(strDirect) && (strncmp(strDirect, "None", 4) == 0 || strncmp(strDirect, "ByteProperty", 12) == 0)) {
                            LOGI("[AutoScanner] >>> SUCCESS: Discovered GNames/FNamePool at 0x%lx (direct str: %s) <<<", addr, strDirect);
                            return addr;
                        }
                    }
                }
            }
        }
        LOGI("[AutoScanner] Heuristic GNames scan did not find a match, using fallback.");
        return 0;
    }

    static uintptr_t ScanGUObjectArrayHeuristic(const char* moduleName, bool& outHasGCHeader) {
        LOGI("[AutoScanner] Initiating Heuristic Scan for GUObjectArray...");
        auto segments = Memory::GetModuleSegments(moduleName);
        
        for (const auto& seg : segments) {
            if (!seg.isReadable || !seg.isWritable) continue;
            
            for (uintptr_t addr = seg.start; addr + 0x40 < seg.end; addr += 8) {
                for (bool hasHeader : {true, false}) {
                    uintptr_t testArrayAddr = hasHeader ? (addr + 0x10) : addr;
                    TUObjectArray* arr = reinterpret_cast<TUObjectArray*>(testArrayAddr);
                    
                    if (arr->NumElements < 200 || arr->NumElements > 2000000) continue;
                    if (arr->MaxElements < arr->NumElements || arr->MaxElements > 5000000) continue;
                    if (arr->NumChunks < 1 || arr->NumChunks > 1000) continue;
                    if (arr->MaxChunks < arr->NumChunks || arr->MaxChunks > 2000) continue;
                    
                    if (!Memory::IsValidPtr(arr->Objects)) continue;
                    if (!Memory::IsValidPtr(arr->Objects[0])) continue;
                    
                    int validObjectCount = 0;
                    int validVTableCount = 0;
                    
                    for (int i = 0; i < std::min(arr->NumElements, 100); ++i) {
                        FUObjectItem* item = &arr->Objects[0][i];
                        if (!Memory::IsValidPtr(item)) break;
                        
                        UObject* obj = item->Object;
                        if (Memory::IsValidPtr(obj)) {
                            validObjectCount++;
                            void** vtbl = reinterpret_cast<void**>(obj->VTable);
                            if (Memory::IsValidPtr(vtbl) && Memory::IsAddressInExecutable(reinterpret_cast<uintptr_t>(vtbl), moduleName)) {
                                validVTableCount++;
                            }
                        }
                    }
                    
                    if (validObjectCount >= 10 && validVTableCount >= 5) {
                        outHasGCHeader = hasHeader;
                        LOGI("[AutoScanner] >>> SUCCESS: Discovered GUObjectArray at 0x%lx (hasGCHeader: %d, NumElements: %d, ValidVTables: %d) <<<",
                             addr, hasHeader ? 1 : 0, arr->NumElements, validVTableCount);
                        return addr;
                    }
                }
            }
        }
        LOGI("[AutoScanner] Heuristic GUObjectArray scan did not find a match, using fallback.");
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
        TUObjectArray* array = reinterpret_cast<TUObjectArray*>(arrayAddr);
        if (!Memory::IsValidPtr(array)) return 0;
        return array->NumElements;
    }

    UObject* CoreManager::GetObjectByIndex(int32_t Index) const {
        if (!GUObjectArrayAddr || Index < 0) return nullptr;
        uintptr_t arrayAddr = bArrayHasGCHeader ? (GUObjectArrayAddr + 0x10) : GUObjectArrayAddr;
        TUObjectArray* array = reinterpret_cast<TUObjectArray*>(arrayAddr);
        if (!Memory::IsValidPtr(array) || Index >= array->NumElements) return nullptr;

        const int32_t ElementsPerChunk = 65536; // 64K objects per chunk
        int32_t ChunkIndex = Index / ElementsPerChunk;
        int32_t InChunkIndex = Index % ElementsPerChunk;

        if (!Memory::IsValidPtr(array->Objects) || !Memory::IsValidPtr(array->Objects[ChunkIndex])) {
            return nullptr;
        }

        FUObjectItem& item = array->Objects[ChunkIndex][InChunkIndex];
        return item.Object;
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
