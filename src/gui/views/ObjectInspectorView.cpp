#include "ObjectInspectorView.hpp"
#include "../config.hpp"
#include "Memory.hpp"
#include "imgui.h"
#include <algorithm>

namespace GUI {

    void ObjectInspectorView::SetTargetObject(UE::UObject* Target) {
        ClearBreadcrumbs();
        if (Target && UE::Memory::IsValidPtr(Target)) {
            PushBreadcrumb(Target);
        }
    }

    void ObjectInspectorView::PushBreadcrumb(UE::UObject* SubObject) {
        if (!SubObject || !UE::Memory::IsValidPtr(SubObject)) return;
        Breadcrumbs.push_back({ SubObject->GetName(), SubObject });
        CurrentObject = SubObject;
    }

    void ObjectInspectorView::PopBreadcrumb() {
        if (Breadcrumbs.size() > 1) {
            Breadcrumbs.pop_back();
            CurrentObject = Breadcrumbs.back().ObjectPtr;
        }
    }

    void ObjectInspectorView::ClearBreadcrumbs() {
        Breadcrumbs.clear();
        CurrentObject = nullptr;
    }

    void ObjectInspectorView::RenderBreadcrumbs() {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Path: ");
        ImGui::SameLine();

        for (size_t i = 0; i < Breadcrumbs.size(); ++i) {
            if (i > 0) {
                ImGui::TextColored(ImVec4(0.8f, 0.3f, 0.6f, 1.0f), " > ");
                ImGui::SameLine();
            }

            bool isLast = (i == Breadcrumbs.size() - 1);
            if (isLast) {
                ImGui::TextColored(ImVec4(0.95f, 0.4f, 0.8f, 1.0f), "%s (%p)", 
                                   Breadcrumbs[i].Name.c_str(), Breadcrumbs[i].ObjectPtr);
            } else {
                if (ImGui::SmallButton(Breadcrumbs[i].Name.c_str())) {
                    // Navigate back to this breadcrumb
                    while (Breadcrumbs.size() > i + 1) {
                        Breadcrumbs.pop_back();
                    }
                    CurrentObject = Breadcrumbs.back().ObjectPtr;
                    break;
                }
            }
            ImGui::SameLine();
        }
        ImGui::NewLine();
    }

    void ObjectInspectorView::RenderPropertiesTable() {
        if (!CurrentObject || !UE::Memory::IsValidPtr(CurrentObject)) {
            ImGui::TextDisabled("No active object being inspected.");
            return;
        }

        UE::UClass* cls = CurrentObject->GetClass();
        if (!cls) {
            ImGui::TextDisabled("Object class is invalid.");
            return;
        }

        auto props = cls->GetProperties();

        // Property Table (Matching Video columns: 字段/属性 | 值 | 类型)
        static ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | 
                                       ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;

        if (ImGui::BeginTable("PropertyTable", 3, flags, ImVec2(0, ImGui::GetContentRegionAvail().y - 45))) {
            ImGui::TableSetupColumn("Field / Property (字段/属性)", ImGuiTableColumnFlags_WidthStretch, 0.4f);
            ImGui::TableSetupColumn("Value (实时值 / 编辑)", ImGuiTableColumnFlags_WidthStretch, 0.4f);
            ImGui::TableSetupColumn("Type (类型)", ImGuiTableColumnFlags_WidthStretch, 0.2f);
            ImGui::TableHeadersRow();

            std::string filter(PropertyFilter);
            std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);

            for (UE::FProperty* prop : props) {
                if (!prop) continue;
                std::string propName = prop->GetName();
                std::string propNameLower = propName;
                std::transform(propNameLower.begin(), propNameLower.end(), propNameLower.begin(), ::tolower);

                if (!filter.empty() && propNameLower.find(filter) == std::string::npos) {
                    continue;
                }

                auto displayInfo = UE::PropertyReader::ReadProperty(CurrentObject, prop);

                ImGui::TableNextRow();
                
                // Column 1: Name
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(displayInfo.Name.c_str());

                // Column 2: Value & Widget
                ImGui::TableSetColumnIndex(1);
                UE::PropertyReader::RenderPropertyWidget(CurrentObject, prop, displayInfo, [this](UE::UObject* subObj) {
                    this->PushBreadcrumb(subObj);
                });

                // Column 3: Type Name
                ImGui::TableSetColumnIndex(2);
                ImGui::TextColored(ImVec4(0.7f, 0.6f, 0.8f, 1.0f), "%s", displayInfo.TypeName.c_str());
            }

            ImGui::EndTable();
        }
    }

    void ObjectInspectorView::Render() {
        if (!CurrentObject || !UE::Memory::IsValidPtr(CurrentObject)) {
            ImGui::TextDisabled("Select an instance from the Browser or enter a valid pointer address.");
            return;
        }

        // Top Controls: Breadcrumb & Options
        ImGui::BeginChild("InspectorHeader", ImVec2(0, 75), true);
        {
            RenderBreadcrumbs();

            ImGui::Checkbox("Auto-Refresh (始终更新)", &bAutoRefresh);
            ImGui::SameLine();
            ImGui::Checkbox("Show Functions (包含方法)", &bShowFunctions);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(180.0f);
            ImGui::InputTextWithHint("##PropFilter", "Filter Properties...", PropertyFilter, sizeof(PropertyFilter));
        }
        ImGui::EndChild();

        ImGui::Spacing();

        // Main Properties View
        RenderPropertiesTable();

        // Bottom Action Bar (Matching video "Dump Object" button)
        ImGui::BeginChild("InspectorBottomBar", ImVec2(0, 38), false);
        {
            if (ImGui::Button("Dump Object (转储到文件)", ImVec2(180, 32))) {
                std::string path = std::string(Config::DUMP_OUTPUT_DIR) + "/" + CurrentObject->GetName() + ".json";
                if (UE::PropertyReader::DumpObjectToFile(CurrentObject, path)) {
                    StatusMessage = "Dumped successfully to: " + path;
                } else {
                    StatusMessage = "Failed to write dump file.";
                }
            }

            if (!StatusMessage.empty()) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.0f), "%s", StatusMessage.c_str());
            }
        }
        ImGui::EndChild();
    }
}
