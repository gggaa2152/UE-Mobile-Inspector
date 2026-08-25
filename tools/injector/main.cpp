#include "ptrace_helper.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

void PrintUsage(const char* progName) {
    printf("====================================================\n");
    printf(" UE Mobile Inspector Native Injector (Android CLI) \n");
    printf("====================================================\n");
    printf("Usage:\n");
    printf("  %s -p <package_name> [-s <so_path>]\n", progName);
    printf("  %s -pid <process_id>  [-s <so_path>]\n", progName);
    printf("\nExamples:\n");
    printf("  %s -p com.tencent.tmgp.dfm\n", progName);
    printf("  %s -pid 12345 -s /data/local/tmp/libUEMobileInspector.so\n", progName);
    printf("====================================================\n");
}

int main(int argc, char** argv) {
    std::string packageName = "";
    std::string soPath = "/data/local/tmp/libUEMobileInspector.so";
    pid_t targetPid = -1;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            packageName = argv[++i];
        } else if (strcmp(argv[i], "-pid") == 0 && i + 1 < argc) {
            targetPid = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            soPath = argv[++i];
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            PrintUsage(argv[0]);
            return 0;
        }
    }

    if (packageName.empty() && targetPid <= 0) {
        PrintUsage(argv[0]);
        return 1;
    }

    // Resolve PID by package name if needed
    if (targetPid <= 0) {
        printf("[*] Searching for process matching '%s'...\n", packageName.c_str());
        targetPid = Injector::FindProcessId(packageName);
        if (targetPid <= 0) {
            printf("[-] Process '%s' not found! Make sure the game is running.\n", packageName.c_str());
            return 1;
        }
    }

    printf("[+] Target process found! PID: %d\n", targetPid);
    printf("[*] Injecting shared library: %s\n", soPath.c_str());

    bool success = Injector::InjectLibrary(targetPid, soPath);
    if (success) {
        printf("[+] Injection completed successfully!\n");
        return 0;
    } else {
        printf("[-] Injection failed!\n");
        return 1;
    }
}
