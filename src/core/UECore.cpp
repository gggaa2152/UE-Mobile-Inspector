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

        // UE4.23+ FNamePool / FNameEntryAllocator layout
        uint32_t block = ComparisonIndex >> 16;
        uint32_t offset = ComparisonIndex & 65535;

        uintptr_t* blocks = reinterpret_cast<uintptr_t*>(gnames + 0x10);
        if (!Memory::IsValidPtr(blocks) || !Memory::IsValidPtr(reinterpret_cast<void*>(blocks[block]))) {
            return "Name_" + std::to_string(ComparisonIndex);
        }

        uintptr_t entry = blocks[block] + offset * 2;
        if (!Memory::IsValidPtr(reinterpret_cast<void*>(entry))) {
            return "Name_" + std::to_string(ComparisonIndex);
        }

        uint16_t header = *reinterpret_cast<uint16_t*>(entry);
        uint32_t len = header >> 6;
        if (len > 0 && len < 1024) {
            char buf[1024] = {0};
            memcpy(buf, reinterpret_cast<const void*>(entry + 2), len);
            buf[len] = '\0';
            std::string result(buf);
            if (Number > 0) {
                result += "_" + std::to_string(Number - 1);
            }
            return result;
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

    bool CoreManager::ResolveOffsets() {
        uintptr_t ueBase = Memory::GetModuleBase(Config::UE_SO_NAME);
        if (!ueBase) {
            LOGE("Cannot find module %s", Config::UE_SO_NAME);
            return false;
        }

        // AOB Scanner for GUObjectArray & GNames / FNamePool
        // Pattern 1: FNamePool (UE4.23 - UE5.x)
        GNamesAddr = Memory::FindPattern(Config::UE_SO_NAME, "\x00\x00\x00\x00\x00\x00\x00\x00\x10\x00\x00\x00", "????????xxxx");
        if (!GNamesAddr) {
            // Fallback to Config offset
            GNamesAddr = ueBase + Config::GNamesOffset;
        }

        // Pattern 2: GUObjectArray
        GUObjectArrayAddr = Memory::FindPattern(Config::UE_SO_NAME, "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x04\x00\x00", "????????xxxx");
        if (!GUObjectArrayAddr) {
            // Fallback to Config offset
            GUObjectArrayAddr = ueBase + Config::GUObjectArrayOffset;
        }

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
        // UE4.25+ FUObjectArray has a 0x10 byte header before the chunked array
        TUObjectArray* array = reinterpret_cast<TUObjectArray*>(GUObjectArrayAddr + 0x10);
        if (!Memory::IsValidPtr(array)) return 0;
        return array->NumElements;
    }

    UObject* CoreManager::GetObjectByIndex(int32_t Index) const {
        if (!GUObjectArrayAddr || Index < 0) return nullptr;
        TUObjectArray* array = reinterpret_cast<TUObjectArray*>(GUObjectArrayAddr + 0x10);
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
