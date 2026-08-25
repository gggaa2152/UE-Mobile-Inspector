#pragma once

#include "UECore.hpp"
#include <string>
#include <vector>
#include <functional>

namespace UE {

    struct PropertyValueDisplay {
        std::string Name;
        std::string TypeName;
        std::string ValueStr;
        int32_t Offset;
        void* RawAddress;
        FProperty* PropertyPtr;
        UObject* SubObjectPtr; // If it's an object property
        bool bIsSubObject;
        bool bIsStruct;
        bool bIsArray;
    };

    class PropertyReader {
    public:
        // Reads property value into human-readable string and display info
        static PropertyValueDisplay ReadProperty(void* Container, FProperty* Property);

        // Render interactive ImGui widget for this property and handle in-place edit
        static bool RenderPropertyWidget(void* Container, FProperty* Property, const PropertyValueDisplay& DisplayInfo, std::function<void(UObject*)> onInspectSubObject);

        // Recursively dump object properties into JSON-like formatted string
        static std::string DumpObjectToString(UObject* Object);
        static bool DumpObjectToFile(UObject* Object, const std::string& FilePath);
    };
}
