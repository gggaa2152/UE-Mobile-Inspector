#include "GUI.hpp"
#include "../config.hpp"
#include "Style.hpp"
#include "views/ClassBrowserView.hpp"
#include "views/ObjectInspectorView.hpp"
#include "views/TracerView.hpp"
#include "views/ToolsView.hpp"
#include "views/SettingsView.hpp"
#include "imgui.h"

namespace GUI {

    void MainGUI::Initialize() {
        if (bInitialized) return;

        Style::ApplyTheme();
        UE::CoreManager::Get().Initialize();

        bInitialized = true;
    }

    void MainGUI::SwitchToInspector(UE::UObject* TargetObject) {
        ObjectInspectorView::Get().SetTargetObject(TargetObject);
        CurrentTab = ActiveTab::Inspector;
    }

    void MainGUI::RenderFloatingButton() {
        // Floating icon to open/close menu on mobile touch
        ImGui::SetNextWindowPos(ImVec2(30, 100), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(60, 60));
        
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize;
        if (ImGui::Begin("##FloatingToggleBtn", nullptr, flags)) {
            if (ImGui::Button("UE", ImVec2(50, 50))) {
                Config::bShowMenu = !Config::bShowMenu;
            }
        }
        ImGui::End();
    }

    void MainGUI::RenderMainWindow() {
        ImGuiIO& io = ImGui::GetIO();
        ImVec2 center = ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
        ImGui::SetNextWindowPos(center, ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x * 0.85f, io.DisplaySize.y * 0.85f), ImGuiCond_FirstUseEver);

        ImGui::SetNextWindowBgAlpha(Config::WindowAlpha);

        if (!ImGui::Begin(TOOL_TAG "###MainWindow", &Config::bShowMenu)) {
            ImGui::End();
            return;
        }

        // Top Tab Bar (Matching video top buttons)
        if (ImGui::BeginTabBar("MainTopTabBar")) {
            if (ImGui::BeginTabItem("浏览器 (Browser)", nullptr, (CurrentTab == ActiveTab::Browser) ? ImGuiTabItemFlags_SetSelected : 0)) {
                CurrentTab = ActiveTab::Browser;
                ClassBrowserView::Get().Render([this](UE::UObject* obj) {
                    this->SwitchToInspector(obj);
                });
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("检查器 (Inspector)", nullptr, (CurrentTab == ActiveTab::Inspector) ? ImGuiTabItemFlags_SetSelected : 0)) {
                CurrentTab = ActiveTab::Inspector;
                ObjectInspectorView::Get().Render();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("追踪 (Tracer)", nullptr, (CurrentTab == ActiveTab::Tracer) ? ImGuiTabItemFlags_SetSelected : 0)) {
                CurrentTab = ActiveTab::Tracer;
                TracerView::Get().Render();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("工具 (Tools)", nullptr, (CurrentTab == ActiveTab::Tools) ? ImGuiTabItemFlags_SetSelected : 0)) {
                CurrentTab = ActiveTab::Tools;
                ToolsView::Get().Render();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("设置 (Settings)", nullptr, (CurrentTab == ActiveTab::Settings) ? ImGuiTabItemFlags_SetSelected : 0)) {
                CurrentTab = ActiveTab::Settings;
                SettingsView::Get().Render();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::End();
    }

    void MainGUI::Render() {
        if (!bInitialized) {
            Initialize();
        }

        RenderFloatingButton();

        if (Config::bShowMenu) {
            RenderMainWindow();
        }
    }
}
