#pragma once

#include "UECore.hpp"
#include <string>
#include <vector>
#include <functional>

namespace GUI {

    class ClassBrowserView {
    public:
        static ClassBrowserView& Get() {
            static ClassBrowserView instance;
            return instance;
        }

        void Render(std::function<void(UE::UObject*)> onSelectInstance);

    private:
        ClassBrowserView() = default;

        char SearchFilter[128] = "";
        char ManualAddressInput[32] = "0x";
        
        bool bFilterInheritance = true;
        bool bFilterWildcard = false;
        bool bFilterFuzzy = true;

        UE::UClass* SelectedClass = nullptr;
        std::vector<UE::UClass*> CachedClasses;
        std::vector<UE::UObject*> FoundInstances;

        bool bClassesCached = false;
        void RefreshClassCache();
    };
}
