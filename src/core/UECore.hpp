#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include "Memory.hpp"

namespace UE {

    class UObject;
    class UClass;
    class UStruct;
    class UFunction;
    class FProperty;

    // FName Structure for UE4.23+ and older
    struct FName {
        uint32_t ComparisonIndex;
        uint32_t Number;

        std::string ToString() const;
    };

    // Forward declare FField for UE4.25+ FProperty
    class FField {
    public:
        void* VTable;
        void* ClassPrivate;
        void* Owner;
        FField* Next;
        FName NamePrivate;
        int32_t Flags;

        std::string GetName() const;
    };

    // Generic Property Representation
    class FProperty : public FField {
    public:
        int32_t ArrayDim;
        int32_t ElementSize;
        uint64_t PropertyFlags;
        uint16_t RepIndex;
        uint16_t RepNotifyIdx;
        int32_t Offset_Internal;

        int32_t GetOffset() const { return Offset_Internal; }
        std::string GetTypeName() const;
        void* ContainerPtrToValueAddress(void* Container) const {
            return reinterpret_cast<void*>(reinterpret_cast<uint8_t*>(Container) + Offset_Internal);
        }
    };

    // Base UObject
    class UObject {
    public:
        void* VTable;
        int32_t ObjectFlags;
        int32_t InternalIndex;
        UClass* ClassPrivate;
        FName NamePrivate;
        UObject* OuterPrivate;

        std::string GetName() const;
        std::string GetFullName() const;
        UClass* GetClass() const { return ClassPrivate; }
        UObject* GetOuter() const { return OuterPrivate; }
        
        bool IsA(UClass* SomeBaseClass) const;
        void ProcessEvent(UFunction* Function, void* Parms);
    };

    // UField & UStruct
    class UField : public UObject {
    public:
        UField* Next;
    };

    class UStruct : public UField {
    public:
        UStruct* SuperStruct;
        UField* Children;
        FField* ChildProperties; // UE4.25+
        int32_t PropertiesSize;
        int32_t MinAlignment;

        std::vector<FProperty*> GetProperties() const;
        std::vector<UFunction*> GetFunctions() const;
    };

    class UClass : public UStruct {
    public:
        uint8_t Pad[0x100]; // Extra class metadata padding
    };

    class UFunction : public UStruct {
    public:
        int32_t FunctionFlags;
        uint8_t NumParms;
        uint16_t ParmsSize;
        uint16_t ReturnValueOffset;
        uint16_t RPCId;
        uint16_t RPCResponseId;
        void* Func; // Native exec function pointer
    };

    // Global Object & Name Management
    class CoreManager {
    public:
        static CoreManager& Get() {
            static CoreManager instance;
            return instance;
        }

        bool Initialize();
        bool IsInitialized() const { return bInitialized; }

        uintptr_t GetGUObjectArrayAddress() const { return GUObjectArrayAddr; }
        uintptr_t GetGNamesAddress() const { return GNamesAddr; }
        uintptr_t GetProcessEventAddress() const { return ProcessEventAddr; }

        int32_t GetObjectCount() const;
        UObject* GetObjectByIndex(int32_t Index) const;

        std::vector<UClass*> GetAllClasses();
        std::vector<UObject*> FindInstancesOfClass(UClass* TargetClass);
        UObject* FindObject(const std::string& FullName);
        UClass* FindClass(const std::string& ClassName);

    private:
        CoreManager() = default;
        bool bInitialized = false;

        uintptr_t GUObjectArrayAddr = 0;
        uintptr_t GNamesAddr = 0;
        uintptr_t ProcessEventAddr = 0;

        bool ResolveOffsets();
    };
}
