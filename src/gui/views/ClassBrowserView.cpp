#include "ClassBrowserView.hpp"
#include "imgui.h"
#include <algorithm>

namespace GUI {

    void ClassBrowserView::RefreshClassCache() {
        CachedClasses = UE::CoreManager::Get().GetAllClasses();
        bClassesCached = true;
    }

    void ClassBrowserView::Render(std::function<void(UE::UObject*)> onSelectInstance) {
        if (!bClassesCached) {
            RefreshClassCache();
        }

        // Top Search & Filter Bar (Matching video layout)
        ImGui::BeginChild("TopFilterBar", ImVec2(0, 70), true);
        {
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 140);
            ImGui::InputTextWithHint("##FilterInput", "Search Class / Object Name...", SearchFilter, sizeof(SearchFilter));
            ImGui::SameLine();
            if (ImGui::Button("Refresh Cache", ImVec2(130, 0))) {
                RefreshClassCache();
            }

            ImGui::Checkbox("Fuzzy Search", &bFilterFuzzy);
            ImGui::SameLine();
            ImGui::Checkbox("Inheritance", &bFilterInheritance);
            ImGui::SameLine();
            ImGui::Checkbox("Wildcard", &bFilterWildcard);
        }
        ImGui::EndChild();

        ImGui::Spacing();

        // Two-Column Layout: Left (Class List) / Right (Instances & Details)
        float contentWidth = ImGui::GetContentRegionAvail().x;
        float leftWidth = contentWidth * 0.45f;
        float rightWidth = contentWidth - leftWidth - 10.0f;

        // Left: Class List
        ImGui::BeginChild("ClassListPanel", ImVec2(leftWidth, 0), true);
        {
            ImGui::TextColored(ImVec4(0.9f, 0.4f, 0.8f, 1.0f), "Available Classes (%zu)", CachedClasses.size());
            ImGui::Separator();

            std::string filterStr(SearchFilter);
            std::transform(filterStr.begin(), filterStr.end(), filterStr.begin(), ::tolower);

            for (UE::UClass* cls : CachedClasses) {
                if (!cls) continue;
                std::string clsName = cls->GetName();
                std::string clsNameLower = clsName;
                std::transform(clsNameLower.begin(), clsNameLower.end(), clsNameLower.begin(), ::tolower);

                if (!filterStr.empty() && clsNameLower.find(filterStr) == std::string::npos) {
                    continue;
                }

                bool isSelected = (SelectedClass == cls);
                if (ImGui::Selectable(clsName.c_str(), isSelected)) {
                    SelectedClass = cls;
                    FoundInstances.clear();
                }
            }
        }
        ImGui::EndChild();

        ImGui::SameLine();

        // Right: Selected Class Operations & Instances
        ImGui::BeginChild("ClassDetailsPanel", ImVec2(rightWidth, 0), true);
        {
            if (SelectedClass) {
                ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.75f, 1.0f), "Class: %s", SelectedClass->GetName().c_str());
                ImGui::Text("Full: %s", SelectedClass->GetFullName().c_str());
                ImGui::Separator();

                // Buttons matching video: "查找实例" / "手动输入"
                if (ImGui::Button("Find Instances (查找实例)", ImVec2(180, 32))) {
                    FoundInstances = UE::CoreManager::Get().FindInstancesOfClass(SelectedClass);
                }
                ImGui::SameLine();
                ImGui::SetNextItemWidth(140.0f);
                ImGui::InputText("##ManualPtr", ManualAddressInput, sizeof(ManualAddressInput));
                ImGui::SameLine();
                if (ImGui::Button("Inspect Address", ImVec2(120, 32))) {
                    uintptr_t ptr = strtoul(ManualAddressInput, nullptr, 16);
                    if (ptr && UE::Memory::IsValidPtr(reinterpret_cast<void*>(ptr))) {
                        if (onSelectInstance) {
                            onSelectInstance(reinterpret_cast<UE::UObject*>(ptr));
                        }
                    }
                }

                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Instances Found (%zu):", FoundInstances.size());
                ImGui::Separator();

                // Instance list
                ImGui::BeginChild("InstanceSubList", ImVec2(0, 0), false);
                for (UE::UObject* inst : FoundInstances) {
                    if (!inst || !UE::Memory::IsValidPtr(inst)) continue;
                    char label[256];
                    snprintf(label, sizeof(label), "%s (0x%lx)", inst->GetName().c_str(), reinterpret_cast<uintptr_t>(inst));
                    
                    if (ImGui::Button(label, ImVec2(ImGui::GetContentRegionAvail().x, 28))) {
                        if (onSelectInstance) {
                            onSelectInstance(inst);
                        }
                    }
                }
                ImGui::EndChild();
            } else {
                ImGui::TextDisabled("Select a class from the left list to view instances and details.");
            }
        }
        ImGui::EndChild();
    }
}
