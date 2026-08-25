#include "TracerView.hpp"
#include "imgui.h"
#include <iomanip>
#include <sstream>
#include <chrono>
#include <ctime>

namespace GUI {

    void TracerView::Render() {
        auto& hook = UE::ProcessEventHook::Get();

        // Control Header
        ImGui::BeginChild("TracerHeader", ImVec2(0, 50), true);
        {
            bool isTracking = hook.IsTracerActive();
            if (isTracking) {
                if (ImGui::Button("Stop Tracer (停止追踪)", ImVec2(160, 32))) {
                    hook.SetTracerActive(false);
                }
            } else {
                if (ImGui::Button("Start Tracer (开始追踪)", ImVec2(160, 32))) {
                    if (!hook.IsEnabled()) {
                        hook.Enable();
                    }
                    hook.SetTracerActive(true);
                }
            }

            ImGui::SameLine();
            if (ImGui::Button("Clear (清空记录)", ImVec2(120, 32))) {
                hook.ClearRecords();
            }

            ImGui::SameLine();
            ImGui::Checkbox("Auto-Scroll", &bAutoScroll);

            ImGui::SameLine();
            ImGui::SetNextItemWidth(200.0f);
            if (ImGui::InputTextWithHint("##TraceFilter", "Filter Function / Object...", FilterInput, sizeof(FilterInput))) {
                hook.SetFilter(FilterInput);
            }
        }
        ImGui::EndChild();

        ImGui::Spacing();

        // Trace Events Log Table
        auto records = hook.GetRecords();

        static ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | 
                                       ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;

        if (ImGui::BeginTable("TraceLogTable", 5, flags, ImVec2(0, 0))) {
            ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("Class (类)", ImGuiTableColumnFlags_WidthStretch, 0.25f);
            ImGui::TableSetupColumn("Object Name (调用对象)", ImGuiTableColumnFlags_WidthStretch, 0.35f);
            ImGui::TableSetupColumn("Function (触发函数)", ImGuiTableColumnFlags_WidthStretch, 0.4f);
            ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed, 110.0f);
            ImGui::TableHeadersRow();

            for (const auto& rec : records) {
                ImGui::TableNextRow();

                // Format Time
                auto in_time_t = std::chrono::system_clock::to_time_t(rec.Timestamp);
                std::stringstream ss;
                ss << std::put_time(std::localtime(&in_time_t), "%H:%M:%S");

                ImGui::TableSetColumnIndex(0);
                ImGui::TextDisabled("%s", ss.str().c_str());

                ImGui::TableSetColumnIndex(1);
                ImGui::TextColored(ImVec4(0.8f, 0.4f, 0.7f, 1.0f), "%s", rec.ObjectClass.c_str());

                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(rec.ObjectName.c_str());

                ImGui::TableSetColumnIndex(3);
                ImGui::TextColored(ImVec4(0.3f, 0.85f, 0.9f, 1.0f), "%s", rec.FunctionName.c_str());

                ImGui::TableSetColumnIndex(4);
                ImGui::TextDisabled("%p", reinterpret_cast<void*>(rec.ObjectAddr));
            }

            if (bAutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                ImGui::SetScrollHereY(1.0f);
            }

            ImGui::EndTable();
        }
    }
}
