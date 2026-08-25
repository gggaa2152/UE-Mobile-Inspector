#include "ptrace_helper.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <vector>

// Helper to check if file exists
static bool FileExists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

// Helper to copy file
static bool CopyFile(const std::string& src, const std::string& dst) {
    FILE* in = fopen(src.c_str(), "rb");
    if (!in) return false;
    FILE* out = fopen(dst.c_str(), "wb");
    if (!out) {
        fclose(in);
        return false;
    }
    char buf[8192];
    size_t n = 0;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        fwrite(buf, 1, n, out);
    }
    fclose(in);
    fclose(out);
    chmod(dst.c_str(), 0777);
    return true;
}

// Auto-scan running processes for Unreal Engine / Game targets without requiring any user input
static pid_t AutoDetectTargetPid(std::string& detectedName) {
    const char* defaultCandidates[] = {
        "com.tencent.tmgp.dfm",         // Delta Force Mobile (三角洲行动)
        "com.tencent.tmgp.pubgm",       // PUBG Mobile CN
        "com.tencent.ig",               // PUBG Mobile Global
        "com.epicgames.portal",         // Fortnite / Unreal Engine Portal
        "com.proximabeta.mf.uamo",      // Arena Breakout
        "com.netease.hyxd",             // Knives Out
        "com.miHoYo",                   // Genshin / Honkai
        nullptr
    };

    // 1. Try finding predefined common UE packages first
    for (int i = 0; defaultCandidates[i] != nullptr; ++i) {
        pid_t pid = Injector::FindProcessId(defaultCandidates[i]);
        if (pid > 0) {
            detectedName = defaultCandidates[i];
            return pid;
        }
    }

    // 2. Deep scan: Look for any running process containing UE modules in /proc/[pid]/maps
    DIR* dir = opendir("/proc");
    if (!dir) return -1;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        int pid = atoi(entry->d_name);
        if (pid <= 100) continue; // Skip kernel/system daemons

        char mapsPath[64];
        snprintf(mapsPath, sizeof(mapsPath), "/proc/%d/maps", pid);
        FILE* fp = fopen(mapsPath, "rt");
        if (!fp) continue;

        char line[512];
        bool isTarget = false;
        while (fgets(line, sizeof(line), fp)) {
            if (strstr(line, "libUE4.so") || strstr(line, "libUE5.so") ||
                strstr(line, "libUnreal.so") || strstr(line, "libdfm.so") ||
                strstr(line, "libShadowTrackerExtra.so")) {
                isTarget = true;
                break;
            }
        }
        fclose(fp);

        if (isTarget) {
            char cmdlinePath[64];
            snprintf(cmdlinePath, sizeof(cmdlinePath), "/proc/%d/cmdline", pid);
            FILE* cmdFp = fopen(cmdlinePath, "rb");
            char cmdline[256] = {0};
            if (cmdFp) {
                fread(cmdline, 1, sizeof(cmdline) - 1, cmdFp);
                fclose(cmdFp);
            }
            closedir(dir);
            detectedName = (cmdline[0] != '\0') ? cmdline : ("PID " + std::to_string(pid));
            return static_cast<pid_t>(pid);
        }
    }
    closedir(dir);
    return -1;
}

int main(int argc, char** argv) {
    printf("====================================================\n");
    printf(" UE Mobile Inspector - Zero-Input Automated Injector \n");
    printf(" Working Directory: /data/1/                        \n");
    printf("====================================================\n");

    // 1. Ensure permissive permissions on /data/1/
    system("chmod -R 777 /data/1 2>/dev/null");
    system("chcon -R u:object_r:apk_data_file:s0 /data/1 2>/dev/null");

    std::string packageName = "";
    std::string soPath = "/data/1/libUEMobileInspector.so";
    pid_t targetPid = -1;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            packageName = argv[++i];
        } else if (strcmp(argv[i], "-pid") == 0 && i + 1 < argc) {
            targetPid = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            soPath = argv[++i];
        }
    }

    // Auto-resolve SO library path if /data/1/libUEMobileInspector.so doesn't exist
    if (!FileExists(soPath)) {
        if (FileExists("./libUEMobileInspector.so")) {
            soPath = "./libUEMobileInspector.so";
        } else if (FileExists("/data/local/tmp/libUEMobileInspector.so")) {
            soPath = "/data/local/tmp/libUEMobileInspector.so";
        }
    }

    if (!FileExists(soPath)) {
        printf("[-] SO library not found at: %s\n", soPath.c_str());
        printf("[!] Please ensure libUEMobileInspector.so is located in /data/1/\n");
        return 1;
    }
    printf("[+] Using target SO library: %s\n", soPath.c_str());

    // Auto-detect target process if not explicitly specified
    std::string targetDesc = packageName;
    if (targetPid <= 0) {
        if (!packageName.empty()) {
            printf("[*] Searching for package '%s'...\n", packageName.c_str());
            targetPid = Injector::FindProcessId(packageName);
        } else {
            printf("[*] Auto-scanning running Unreal Engine games...\n");
            targetPid = AutoDetectTargetPid(targetDesc);
        }
    }

    if (targetPid <= 0) {
        printf("[-] Target process not found!\n");
        printf("[!] Please make sure your game (e.g. 三角洲行动 / Delta Force Mobile) is open and running on screen.\n");
        return 1;
    }

    printf("[+] Target process found! [%s] (PID: %d)\n", targetDesc.c_str(), targetPid);

    // *** Clear old log files BEFORE injection so we only see fresh output ***
    remove("/data/1/ue_inspector.log");
    remove("/sdcard/ue_inspector.log");
    remove("/data/local/tmp/ue_inspector.log");

    // *** Use unique timestamped filename to force dlopen to ALWAYS reload fresh ***
    // This prevents the bug where dlopen returns an existing handle (stale old code)
    // without re-running the constructor when injecting into an already-injected process.
    long ts = (long)time(nullptr);

    std::vector<std::string> candidatePaths;
    if (targetDesc.find('.') != std::string::npos) {
        // Clean up any old injected SOs from previous runs
        char cleanCmd[512];
        snprintf(cleanCmd, sizeof(cleanCmd), "rm -f /data/data/%s/libUEMI_*.so 2>/dev/null", targetDesc.c_str());
        system(cleanCmd);
        snprintf(cleanCmd, sizeof(cleanCmd), "rm -f /data/user/0/%s/libUEMI_*.so 2>/dev/null", targetDesc.c_str());
        system(cleanCmd);

        // Create uniquely-named copy so dlopen always treats it as a NEW library
        char uniqueName[256];
        snprintf(uniqueName, sizeof(uniqueName), "libUEMI_%ld.so", ts);

        std::string appDir1 = "/data/data/" + targetDesc + "/" + uniqueName;
        std::string appDir2 = "/data/user/0/" + targetDesc + "/" + uniqueName;
        
        CopyFile(soPath, appDir1);
        CopyFile(soPath, appDir2);
        
        char cmdBuf[512];
        snprintf(cmdBuf, sizeof(cmdBuf), "chmod 777 '%s' 2>/dev/null", appDir1.c_str());
        system(cmdBuf);
        snprintf(cmdBuf, sizeof(cmdBuf), "chcon u:object_r:app_data_file:s0 '%s' 2>/dev/null", appDir1.c_str());
        system(cmdBuf);
        snprintf(cmdBuf, sizeof(cmdBuf), "chmod 777 '%s' 2>/dev/null", appDir2.c_str());
        system(cmdBuf);
        snprintf(cmdBuf, sizeof(cmdBuf), "chcon u:object_r:app_data_file:s0 '%s' 2>/dev/null", appDir2.c_str());
        system(cmdBuf);

        printf("[*] Using unique SO name: %s (prevents stale dlopen cache)\n", uniqueName);
        candidatePaths.push_back(appDir1);
        candidatePaths.push_back(appDir2);
    }
    candidatePaths.push_back(soPath);

    printf("[*] Starting automated PTrace injection...\n");

    bool success = false;
    for (const auto& path : candidatePaths) {
        printf("[*] Attempting injection with path: %s\n", path.c_str());
        if (Injector::InjectLibrary(targetPid, path)) {
            success = true;
            break;
        }
    }

    if (success) {
        printf("\n====================================================\n");
        printf(" >>> INJECTION SUCCESSFUL! <<<\n");
        printf("====================================================\n");
        printf("[*] Waiting 8 seconds for in-game diagnostics (including 5s watchdog)...\n");
        sleep(8);

        // Read and display FRESH in-game logs
        const char* logPaths[] = { "/data/1/ue_inspector.log", "/sdcard/ue_inspector.log", "/data/local/tmp/ue_inspector.log" };
        bool printedLogs = false;
        for (const char* lp : logPaths) {
            FILE* lfp = fopen(lp, "rt");
            if (lfp) {
                printf("\n========== [FRESH In-Game Diagnostic Log: %s] ==========\n", lp);
                char line[512];
                while (fgets(line, sizeof(line), lfp)) {
                    printf("%s", line);
                }
                fclose(lfp);
                printf("==========================================================\n\n");
                printedLogs = true;
                break;
            }
        }
        if (!printedLogs) {
            printf("[!] WARNING: No log file was created! The library constructor may have crashed.\n");
            printf("[!] Try: logcat -s UE-Mobile-Inspector\n");
        }
        return 0;
    } else {
        printf("\n[-] Injection failed. Check Root permissions and SELinux (setenforce 0).\n");
        return 1;
    }
}

