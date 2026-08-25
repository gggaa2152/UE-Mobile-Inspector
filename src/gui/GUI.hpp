#pragma once

#include "UECore.hpp"

namespace GUI {

    enum class ActiveTab {
        Browser = 0,
        Inspector = 1,
        Tracer = 2,
        Tools = 3,
        Settings = 4
    };

    class MainGUI {
    public:
        static MainGUI& Get() {
            static MainGUI instance;
            return instance;
        }

        void Initialize();
        void Render();

        void SwitchToInspector(UE::UObject* TargetObject);

    private:
        MainGUI() = default;
        bool bInitialized = false;
        ActiveTab CurrentTab = ActiveTab::Browser;

        void RenderFloatingButton();
        void RenderMainWindow();
    };
}
