#pragma once

#include "ProcessEventHook.hpp"
#include <string>
#include <vector>

namespace GUI {

    class TracerView {
    public:
        static TracerView& Get() {
            static TracerView instance;
            return instance;
        }

        void Render();

    private:
        TracerView() = default;

        char FilterInput[128] = "";
        bool bAutoScroll = true;
    };
}
