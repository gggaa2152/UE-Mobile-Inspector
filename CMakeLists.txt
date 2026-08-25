cmake_minimum_required(VERSION 3.18.0)
project(UEMobileInspector LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optimization & Compiler Flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable -fvisibility=hidden -fvisibility-inlines-hidden -ffunction-sections -fdata-sections")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -flto -DNDEBUG")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--gc-sections -Wl,--exclude-libs,ALL")

# Include Directories
include_directories(
    src
    src/core
    src/gui
    src/gui/views
    src/hook
    src/thirdparty
    src/thirdparty/imgui
    src/thirdparty/dobby
)

# ImGui Source Files
set(IMGUI_SOURCES
    src/thirdparty/imgui/imgui.cpp
    src/thirdparty/imgui/imgui_draw.cpp
    src/thirdparty/imgui/imgui_tables.cpp
    src/thirdparty/imgui/imgui_widgets.cpp
    src/thirdparty/imgui/imgui_demo.cpp
    src/thirdparty/imgui/imgui_impl_opengl3.cpp
    src/thirdparty/imgui/imgui_impl_android.cpp
)

# Core & Feature Sources
set(PROJECT_SOURCES
    src/main.cpp
    src/core/Memory.cpp
    src/core/UECore.cpp
    src/core/UEPropertyReader.cpp
    src/core/ProcessEventHook.cpp
    src/gui/GUI.cpp
    src/gui/views/ClassBrowserView.cpp
    src/gui/views/ObjectInspectorView.cpp
    src/gui/views/TracerView.cpp
    src/gui/views/ToolsView.cpp
    src/gui/views/SettingsView.cpp
    src/hook/EGLHook.cpp
    src/hook/TouchHook.cpp
    ${IMGUI_SOURCES}
)

# Build Target: libUEMobileInspector.so
add_library(UEMobileInspector SHARED ${PROJECT_SOURCES})

# Target Specific Definitions
target_compile_definitions(UEMobileInspector PRIVATE
    IMGUI_IMPL_OPENGL_ES3
    IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    IMGUI_DISABLE_OBSOLETE_KEYIO
)

# Link Libraries for Android
target_link_libraries(UEMobileInspector PRIVATE
    android
    log
    EGL
    GLESv3
    GLESv2
)
