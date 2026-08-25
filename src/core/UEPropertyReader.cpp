#include "UEPropertyReader.hpp"
#include "imgui.h"
#include <fstream>
#include <sstream>
#include <iomanip>

namespace UE {

    // Simple FString representation in UE
    struct FString {
        const wchar_t* Data;
        int32_t ArrayNum;
        int32_t ArrayMax;

        std::string ToString() const {
            if (!Data || ArrayNum <= 0) return "";
            std::wstring ws(Data);
            return std::string(ws.begin(), ws.end());
        }
    };

    struct FVector {
        float X, Y, Z;
    };

    struct FRotator {
        float Pitch, Yaw, Roll;
    };

    PropertyValueDisplay PropertyReader::ReadProperty(void* Container, FProperty* Property) {
        PropertyValueDisplay disp;
        if (!Container || !Property) return disp;

        disp.Name = Property->GetName();
        disp.TypeName = Property->GetTypeName();
        disp.Offset = Property->GetOffset();
        disp.PropertyPtr = Property;
        disp.RawAddress = Property->ContainerPtrToValueAddress(Container);
        disp.SubObjectPtr = nullptr;
        disp.bIsSubObject = false;
        disp.bIsStruct = false;
        disp.bIsArray = false;

        void* valAddr = disp.RawAddress;
        if (!Memory::IsValidPtr(valAddr)) {
            disp.ValueStr = "<Invalid Address>";
            return disp;
        }

        std::string type = disp.TypeName;

        if (type == "BoolProperty") {
            bool val = *reinterpret_cast<bool*>(valAddr);
            disp.ValueStr = val ? "True" : "False";
        }
        else if (type == "IntProperty") {
            disp.ValueStr = std::to_string(*reinterpret_cast<int32_t*>(valAddr));
        }
        else if (type == "Int64Property") {
            disp.ValueStr = std::to_string(*reinterpret_cast<int64_t*>(valAddr));
        }
        else if (type == "FloatProperty") {
            std::stringstream ss;
            ss << std::fixed << std::setprecision(3) << *reinterpret_cast<float*>(valAddr);
            disp.ValueStr = ss.str();
        }
        else if (type == "DoubleProperty") {
            std::stringstream ss;
            ss << std::fixed << std::setprecision(4) << *reinterpret_cast<double*>(valAddr);
            disp.ValueStr = ss.str();
        }
        else if (type == "ByteProperty" || type == "EnumProperty") {
            disp.ValueStr = std::to_string(static_cast<int>(*reinterpret_cast<uint8_t*>(valAddr)));
        }
        else if (type == "NameProperty") {
            FName* name = reinterpret_cast<FName*>(valAddr);
            disp.ValueStr = name->ToString();
        }
        else if (type == "StrProperty") {
            FString* str = reinterpret_cast<FString*>(valAddr);
            disp.ValueStr = "\"" + str->ToString() + "\"";
        }
        else if (type == "ObjectProperty" || type == "WeakObjectPtrProperty") {
            disp.bIsSubObject = true;
            UObject* subObj = *reinterpret_cast<UObject**>(valAddr);
            disp.SubObjectPtr = subObj;
            if (subObj && Memory::IsValidPtr(subObj)) {
                disp.ValueStr = subObj->GetName() + " (" + (subObj->ClassPrivate ? subObj->ClassPrivate->GetName() : "Object") + ")";
            } else {
                disp.ValueStr = "Null";
            }
        }
        else if (type == "StructProperty") {
            disp.bIsStruct = true;
            disp.ValueStr = "<Struct Details>";
        }
        else if (type == "ArrayProperty") {
            disp.bIsArray = true;
            disp.ValueStr = "<TArray>";
        }
        else {
            disp.ValueStr = "<" + type + ">";
        }

        return disp;
    }

    bool PropertyReader::RenderPropertyWidget(void* Container, FProperty* Property, const PropertyValueDisplay& DisplayInfo, std::function<void(UObject*)> onInspectSubObject) {
        if (!Container || !Property || !DisplayInfo.RawAddress) return false;
        void* valAddr = DisplayInfo.RawAddress;

        std::string type = DisplayInfo.TypeName;
        std::string label = "##" + DisplayInfo.Name + "_" + std::to_string(DisplayInfo.Offset);

        if (type == "BoolProperty") {
            bool val = *reinterpret_cast<bool*>(valAddr);
            if (ImGui::Checkbox(label.c_str(), &val)) {
                *reinterpret_cast<bool*>(valAddr) = val;
                return true;
            }
            ImGui::SameLine();
            ImGui::TextColored(val ? ImVec4(0.3f, 0.9f, 0.3f, 1.0f) : ImVec4(0.9f, 0.3f, 0.3f, 1.0f), val ? "True" : "False");
        }
        else if (type == "IntProperty") {
            int32_t val = *reinterpret_cast<int32_t*>(valAddr);
            ImGui::SetNextItemWidth(120.0f);
            if (ImGui::InputInt(label.c_str(), &val, 1, 100)) {
                *reinterpret_cast<int32_t*>(valAddr) = val;
                return true;
            }
        }
        else if (type == "FloatProperty") {
            float val = *reinterpret_cast<float*>(valAddr);
            ImGui::SetNextItemWidth(120.0f);
            if (ImGui::DragFloat(label.c_str(), &val, 0.1f)) {
                *reinterpret_cast<float*>(valAddr) = val;
                return true;
            }
        }
        else if (DisplayInfo.bIsSubObject) {
            if (DisplayInfo.SubObjectPtr && Memory::IsValidPtr(DisplayInfo.SubObjectPtr)) {
                if (ImGui::Button((DisplayInfo.ValueStr + "##btn_" + DisplayInfo.Name).c_str())) {
                    if (onInspectSubObject) {
                        onInspectSubObject(DisplayInfo.SubObjectPtr);
                    }
                }
            } else {
                ImGui::TextDisabled("Null");
            }
        }
        else {
            ImGui::TextUnformatted(DisplayInfo.ValueStr.c_str());
        }

        return false;
    }

    std::string PropertyReader::DumpObjectToString(UObject* Object) {
        if (!Object || !Memory::IsValidPtr(Object)) return "{}";
        UClass* cls = Object->GetClass();
        if (!cls) return "{}";

        std::stringstream ss;
        ss << "{\n";
        ss << "  \"Name\": \"" << Object->GetName() << "\",\n";
        ss << "  \"Class\": \"" << cls->GetName() << "\",\n";
        ss << "  \"Address\": \"0x" << std::hex << reinterpret_cast<uintptr_t>(Object) << "\",\n";
        ss << "  \"Properties\": {\n";

        auto props = cls->GetProperties();
        for (size_t i = 0; i < props.size(); ++i) {
            auto disp = ReadProperty(Object, props[i]);
            ss << "    \"" << disp.Name << "\": {\"Type\": \"" << disp.TypeName << "\", \"Value\": \"" << disp.ValueStr << "\", \"Offset\": " << std::dec << disp.Offset << "}";
            if (i + 1 < props.size()) ss << ",";
            ss << "\n";
        }

        ss << "  }\n}";
        return ss.str();
    }

    bool PropertyReader::DumpObjectToFile(UObject* Object, const std::string& FilePath) {
        std::string jsonStr = DumpObjectToString(Object);
        std::ofstream outFile(FilePath);
        if (!outFile.is_open()) return false;
        outFile << jsonStr;
        outFile.close();
        return true;
    }
}
