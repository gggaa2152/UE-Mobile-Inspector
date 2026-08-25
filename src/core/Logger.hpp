#pragma once

#include <android/log.h>
#include <cstdio>
#include <cstdarg>

#define LOG_TAG "UE-Mobile-Inspector"

namespace Logger {
    void Log(const char* fmt, ...);
}

#define LOGI(...) Logger::Log(__VA_ARGS__)
#define LOGE(...) Logger::Log(__VA_ARGS__)
