#pragma once

#include <string>

namespace GUI {

    class ToolsView {
    public:
        static ToolsView& Get() {
            static ToolsView instance;
            return instance;
        }

        void Render();

    private:
        ToolsView() = default;

        char ConsoleCmdInput[256] = "";
        char SearchPatternInput[128] = "";
        char SearchMaskInput[64] = "";
        std::string StatusMsg = "";
    };
}
