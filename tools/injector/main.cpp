#include "ptrace_helper.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <vector>

// Helper to check if file exists
static bool FileExists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
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
    printf("[*] Starting automated PTrace injection...\n");

    bool success = Injector::InjectLibrary(targetPid, soPath);
    if (success) {
        printf("\n====================================================\n");
        printf(" >>> INJECTION SUCCESSFUL! <<<\n");
        printf(" Look at your phone screen: the [UE] floating icon\n");
        printf(" is now active on the top-left corner.\n");
        printf("====================================================\n");
        return 0;
    } else {
        printf("\n[-] Injection failed. Check Root permissions and SELinux (setenforce 0).\n");
        return 1;
    }
}
