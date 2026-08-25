#pragma once

namespace GUI {

    class SettingsView {
    public:
        static SettingsView& Get() {
            static SettingsView instance;
            return instance;
        }

        void Render();

    private:
        SettingsView() = default;
    };
}
