#pragma once

#include "UECore.hpp"
#include "UEPropertyReader.hpp"
#include <string>
#include <vector>

namespace GUI {

    struct BreadcrumbNode {
        std::string Name;
        UE::UObject* ObjectPtr;
    };

    class ObjectInspectorView {
    public:
        static ObjectInspectorView& Get() {
            static ObjectInspectorView instance;
            return instance;
        }

        void SetTargetObject(UE::UObject* Target);
        void PushBreadcrumb(UE::UObject* SubObject);
        void PopBreadcrumb();
        void ClearBreadcrumbs();

        void Render();

    private:
        ObjectInspectorView() = default;

        std::vector<BreadcrumbNode> Breadcrumbs;
        UE::UObject* CurrentObject = nullptr;

        char PropertyFilter[64] = "";
        bool bAutoRefresh = true;
        bool bShowFunctions = false;
        std::string StatusMessage = "";

        void RenderBreadcrumbs();
        void RenderPropertiesTable();
        void RenderFunctionsSection();
    };
}
