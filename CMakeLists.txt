cmake_minimum_required(VERSION 3.18.0)
project(UEMobileInspector LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler Flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable -fvisibility=hidden -fvisibility-inlines-hidden -ffunction-sections -fdata-sections -static-libstdc++")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--gc-sections -static-libstdc++")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections -static-libstdc++")

# Include Directories
include_directories(
    src
    src/core
    src/gui
    src/gui/views
    src/hook
    src/thirdparty/imgui
    src/thirdparty/imgui/backends
)

# ImGui Source Files
set(IMGUI_SOURCES
    src/thirdparty/imgui/imgui.cpp
    src/thirdparty/imgui/imgui_draw.cpp
    src/thirdparty/imgui/imgui_tables.cpp
    src/thirdparty/imgui/imgui_widgets.cpp
    src/thirdparty/imgui/backends/imgui_impl_opengl3.cpp
    src/thirdparty/imgui/backends/imgui_impl_android.cpp
)

# Core & Feature Sources
set(PROJECT_SOURCES
    src/main.cpp
    src/core/Logger.cpp
    src/core/Memory.cpp
    src/core/UECore.cpp
    src/core/UEPropertyReader.cpp
    src/core/ProcessEventHook.cpp
    src/gui/GUI.cpp
    src/gui/AndroidOverlay.cpp
    src/gui/views/ClassBrowserView.cpp
    src/gui/views/ObjectInspectorView.cpp
    src/gui/views/TracerView.cpp
    src/gui/views/ToolsView.cpp
    src/gui/views/SettingsView.cpp
    src/hook/HookManager.cpp
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

# Link Libraries for shared library
target_link_libraries(UEMobileInspector PRIVATE
    android
    log
    EGL
    GLESv3
    GLESv2
)

# Build Companion Injector Executable: ue_injector
add_executable(ue_injector
    tools/injector/main.cpp
    tools/injector/ptrace_helper.cpp
)

target_include_directories(ue_injector PRIVATE
    tools/injector
)

target_link_libraries(ue_injector PRIVATE
    log
    dl
)
