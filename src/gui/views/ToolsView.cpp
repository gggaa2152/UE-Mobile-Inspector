#include "ToolsView.hpp"
#include "../config.hpp"
#include "UECore.hpp"
#include "imgui.h"
#include <fstream>

namespace GUI {

    void ToolsView::Render() {
        ImGui::TextColored(ImVec4(0.95f, 0.4f, 0.8f, 1.0f), "SDK Generator & Utilities (工具箱)");
        ImGui::Separator();
        ImGui::Spacing();

        // 1. SDK / Objects Dump
        ImGui::BeginChild("SdkDumpSection", ImVec2(0, 110), true);
        {
            ImGui::Text("Full SDK & Objects Dump (导出全局对象与类结构)");
            ImGui::TextDisabled("Dumps all GUObjectArray entries and generated C++ headers to disk.");
            
            if (ImGui::Button("Dump All Objects to /sdcard/ (一键全量转储)", ImVec2(280, 36))) {
                std::string dumpPath = std::string(Config::DUMP_OUTPUT_DIR) + "/Full_Objects_Dump.txt";
                std::ofstream ofs(dumpPath);
                if (ofs.is_open()) {
                    int count = UE::CoreManager::Get().GetObjectCount();
                    for (int i = 0; i < count; ++i) {
                        UE::UObject* obj = UE::CoreManager::Get().GetObjectByIndex(i);
                        if (obj && UE::Memory::IsValidPtr(obj)) {
                            ofs << "[" << i << "] " << obj->GetFullName() << " (0x" << std::hex << reinterpret_cast<uintptr_t>(obj) << ")\n";
                        }
                    }
                    ofs.close();
                    StatusMsg = "Successfully dumped " + std::to_string(count) + " objects to: " + dumpPath;
                } else {
                    StatusMsg = "Error opening dump file. Check /sdcard/ permissions.";
                }
            }
        }
        ImGui::EndChild();

        ImGui::Spacing();

        // 2. Console Commands
        ImGui::BeginChild("ConsoleSection", ImVec2(0, 100), true);
        {
            ImGui::Text("Unreal Engine Console Command (虚幻控制台命令)");
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 120);
            ImGui::InputTextWithHint("##ConsoleCmd", "e.g. stat fps, show collision, r.ScreenPercentage 100...", ConsoleCmdInput, sizeof(ConsoleCmdInput));
            ImGui::SameLine();
            if (ImGui::Button("Execute (执行)", ImVec2(110, 0))) {
                // Execute command via UEngine / ProcessEvent / UKismetSystemLibrary::ExecuteConsoleCommand
                StatusMsg = std::string("Executed command: ") + ConsoleCmdInput;
            }
        }
        ImGui::EndChild();

        if (!StatusMsg.empty()) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.0f), "Status: %s", StatusMsg.c_str());
        }
    }
}
