#include "SettingsView.hpp"
#include "../config.hpp"
#include "imgui.h"

namespace GUI {

    void SettingsView::Render() {
        ImGui::TextColored(ImVec4(0.95f, 0.4f, 0.8f, 1.0f), "Settings & Preferences (设置)");
        ImGui::Separator();
        ImGui::Spacing();

        // Display & Touch Scale
        ImGui::Text("UI & Rendering (界面与显示)");
        ImGui::SliderFloat("UI Scale (触屏缩放)", &Config::MenuScale, 0.5f, 2.5f, "%.1fx");
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            ImGui::GetIO().FontGlobalScale = Config::MenuScale;
        }

        ImGui::SliderFloat("Window Opacity (窗口透明度)", &Config::WindowAlpha, 0.2f, 1.0f, "%.2f");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Inspector Configuration
        ImGui::Text("Inspector Options (检查器配置)");
        ImGui::Checkbox("Auto-Refresh Enabled by Default (默认开启始终更新)", &Config::bAutoRefreshInspector);
        ImGui::SliderInt("Refresh Interval (ms) (刷新间隔)", &Config::RefreshIntervalMs, 16, 1000);
        ImGui::Checkbox("Include Super Class Properties (包含基类继承属性)", &Config::bIncludeSuperProperties);
        ImGui::Checkbox("Show Functions in Inspector (在检查器中列出函数)", &Config::bShowFunctionsInInspector);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Version Info
        ImGui::TextDisabled("UE Mobile Inspector by gggaa2152");
        ImGui::TextDisabled("Tool Version: %s", TOOL_VERSION);
    }
}
