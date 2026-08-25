#include "Logger.hpp"
#include <cstdio>
#include <cstdarg>
#include <ctime>

namespace Logger {
    void Log(const char* fmt, ...) {
        char buf[1024];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);

        // 1. Android Logcat
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "%s", buf);

        // 2. Disk Log Files
        const char* logPaths[] = { 
            "/sdcard/ue_inspector.log", 
            "/data/1/ue_inspector.log", 
            "/data/local/tmp/ue_inspector.log" 
        };

        for (const char* path : logPaths) {
            FILE* fp = fopen(path, "a+");
            if (fp) {
                time_t now = time(nullptr);
                struct tm* t = localtime(&now);
                char timeStr[32] = {0};
                if (t) {
                    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", t);
                }
                fprintf(fp, "[%s][UE-Mobile-Inspector] %s\n", timeStr, buf);
                fflush(fp);
                fclose(fp);
            }
        }
    }
}
