#pragma once

#include <string>
#include <cstdint>

#define TOOL_NAME "UE Mobile Inspector"
#define TOOL_VERSION "v1.0.0-UE"
#define TOOL_TAG "[SV] " TOOL_NAME " " TOOL_VERSION

namespace Config {
    // UI Settings
    inline bool bShowMenu = true;
    inline float MenuScale = 1.0f;
    inline float WindowAlpha = 0.95f;
    inline bool bAutoRefreshInspector = true;
    inline int RefreshIntervalMs = 100;
    inline bool bIncludeSuperProperties = true;
    inline bool bShowFunctionsInInspector = true;
    inline bool bFilterFuzzy = true;

    // Engine Offsets & Compatibility
    inline int EngineVersionMajor = 4; // 4 or 5
    inline int EngineVersionMinor = 27;

    // Process & Memory Paths
    inline const char* UE_SO_NAME = "libUE4.so"; // or libUnreal.so / libUE5.so
    inline const char* DUMP_OUTPUT_DIR = "/sdcard/UE_Inspector_Dumps";
}
