#pragma once

#include "imgui.h"

namespace Style {
    // 1:1 Color scheme from the video: Dark modern with vibrant magenta/purple accents
    inline void ApplyTheme() {
        ImGuiStyle& style = ImGui::GetStyle();
        
        style.WindowRounding = 6.0f;
        style.ChildRounding = 4.0f;
        style.FrameRounding = 4.0f;
        style.PopupRounding = 4.0f;
        style.ScrollbarRounding = 4.0f;
        style.GrabRounding = 4.0f;
        style.TabRounding = 4.0f;
        
        style.WindowBorderSize = 1.0f;
        style.FrameBorderSize = 0.0f;
        style.PopupBorderSize = 1.0f;
        
        style.ItemSpacing = ImVec2(8.0f, 6.0f);
        style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
        style.TouchExtraPadding = ImVec2(4.0f, 4.0f);
        style.IndentSpacing = 20.0f;
        style.ScrollbarSize = 14.0f;

        ImVec4* colors = style.Colors;
        
        // Deep Plum Backgrounds
        colors[ImGuiCol_Text]                  = ImVec4(0.95f, 0.95f, 0.97f, 1.00f);
        colors[ImGuiCol_TextDisabled]          = ImVec4(0.55f, 0.50f, 0.58f, 1.00f);
        colors[ImGuiCol_WindowBg]              = ImVec4(0.12f, 0.08f, 0.14f, 0.96f);
        colors[ImGuiCol_ChildBg]               = ImVec4(0.16f, 0.11f, 0.18f, 0.90f);
        colors[ImGuiCol_PopupBg]               = ImVec4(0.14f, 0.09f, 0.16f, 0.98f);
        colors[ImGuiCol_Border]                = ImVec4(0.35f, 0.18f, 0.32f, 0.60f);
        colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        
        // Frame Backgrounds (Inputs, boxes)
        colors[ImGuiCol_FrameBg]               = ImVec4(0.22f, 0.14f, 0.24f, 0.80f);
        colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.30f, 0.18f, 0.32f, 0.90f);
        colors[ImGuiCol_FrameBgActive]         = ImVec4(0.38f, 0.20f, 0.40f, 1.00f);
        
        // Title Bar & Headers
        colors[ImGuiCol_TitleBg]               = ImVec4(0.18f, 0.10f, 0.20f, 1.00f);
        colors[ImGuiCol_TitleBgActive]         = ImVec4(0.45f, 0.12f, 0.35f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.10f, 0.06f, 0.12f, 0.75f);
        colors[ImGuiCol_MenuBarBg]             = ImVec4(0.16f, 0.10f, 0.18f, 1.00f);
        
        // Scrollbar & Sliders
        colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.10f, 0.06f, 0.12f, 0.60f);
        colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.48f, 0.18f, 0.42f, 0.80f);
        colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.65f, 0.22f, 0.55f, 0.90f);
        colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.85f, 0.25f, 0.70f, 1.00f);
        colors[ImGuiCol_CheckMark]             = ImVec4(0.95f, 0.28f, 0.65f, 1.00f);
        colors[ImGuiCol_SliderGrab]            = ImVec4(0.85f, 0.25f, 0.70f, 0.90f);
        colors[ImGuiCol_SliderGrabActive]      = ImVec4(1.00f, 0.35f, 0.80f, 1.00f);
        
        // Buttons (Vibrant Magenta Accent)
        colors[ImGuiCol_Button]                = ImVec4(0.55f, 0.14f, 0.40f, 0.85f);
        colors[ImGuiCol_ButtonHovered]         = ImVec4(0.75f, 0.18f, 0.55f, 0.95f);
        colors[ImGuiCol_ButtonActive]          = ImVec4(0.90f, 0.22f, 0.68f, 1.00f);
        
        // Headers (Collapsing headers, tree nodes)
        colors[ImGuiCol_Header]                = ImVec4(0.42f, 0.12f, 0.34f, 0.75f);
        colors[ImGuiCol_HeaderHovered]         = ImVec4(0.60f, 0.16f, 0.48f, 0.85f);
        colors[ImGuiCol_HeaderActive]          = ImVec4(0.78f, 0.20f, 0.62f, 1.00f);
        
        // Separators & Resize Grip
        colors[ImGuiCol_Separator]             = ImVec4(0.38f, 0.20f, 0.35f, 0.60f);
        colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.65f, 0.25f, 0.58f, 0.80f);
        colors[ImGuiCol_SeparatorActive]       = ImVec4(0.88f, 0.30f, 0.75f, 1.00f);
        colors[ImGuiCol_ResizeGrip]            = ImVec4(0.55f, 0.15f, 0.45f, 0.40f);
        colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.75f, 0.20f, 0.60f, 0.70f);
        colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.95f, 0.25f, 0.75f, 0.90f);
        
        // Tabs (Matching video top tab bar)
        colors[ImGuiCol_Tab]                   = ImVec4(0.24f, 0.12f, 0.22f, 0.86f);
        colors[ImGuiCol_TabHovered]            = ImVec4(0.65f, 0.18f, 0.48f, 0.90f);
        colors[ImGuiCol_TabActive]             = ImVec4(0.78f, 0.18f, 0.56f, 1.00f);
        colors[ImGuiCol_TabUnfocused]          = ImVec4(0.18f, 0.08f, 0.16f, 0.97f);
        colors[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.42f, 0.12f, 0.32f, 1.00f);
        
        // Table Rows
        colors[ImGuiCol_TableHeaderBg]         = ImVec4(0.26f, 0.12f, 0.24f, 1.00f);
        colors[ImGuiCol_TableBorderStrong]     = ImVec4(0.45f, 0.20f, 0.40f, 0.80f);
        colors[ImGuiCol_TableBorderLight]      = ImVec4(0.32f, 0.15f, 0.30f, 0.50f);
        colors[ImGuiCol_TableRowBg]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_TableRowBgAlt]         = ImVec4(1.00f, 1.00f, 1.00f, 0.04f);
    }
}
