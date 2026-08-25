cmake_minimum_required(VERSION 3.18.0)
project(UEMobileInspector LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Include FetchContent for Dear ImGui
include(FetchContent)

# Fetch Dear ImGui (v1.90.4)
message(STATUS "Fetching Dear ImGui...")
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.90.4
    GIT_SHALLOW TRUE
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()

# Compiler Flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable -fvisibility=hidden -fvisibility-inlines-hidden -ffunction-sections -fdata-sections")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--gc-sections")

# Include Directories
include_directories(
    src
    src/core
    src/gui
    src/gui/views
    src/hook
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

# ImGui Source Files
set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_android.cpp
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

# Link Libraries
target_link_libraries(UEMobileInspector PRIVATE
    android
    log
    EGL
    GLESv3
    GLESv2
)
