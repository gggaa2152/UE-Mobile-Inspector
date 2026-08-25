#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
UE Mobile Inspector - One-Click ADB Injector Script
Usage:
    python tools/inject.py [-p <package_name>] [-d <device_id>]
"""

import sys
import os
import subprocess
import time
import argparse

DEFAULT_PACKAGE = "com.tencent.tmgp.dfm" # Delta Force Mobile or other UE games
REMOTE_TMP = "/data/local/tmp"
LOCAL_SO_NAME = "libUEMobileInspector.so"
LOCAL_INJECTOR_NAME = "ue_injector"

def run_cmd(cmd, check=True):
    res = subprocess.run(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if check and res.returncode != 0:
        print(f"[-] Command failed: {cmd}\n[Error] {res.stderr.strip()}")
    return res.stdout.strip(), res.returncode

def main():
    parser = argparse.ArgumentParser(description="UE Mobile Inspector One-Click Injector")
    parser.add_argument("-p", "--package", default=DEFAULT_PACKAGE, help="Target game package name (default: com.tencent.tmgp.dfm)")
    parser.add_argument("-s", "--so", default="", help="Path to libUEMobileInspector.so")
    parser.add_argument("-i", "--injector", default="", help="Path to ue_injector binary")
    parser.add_argument("-d", "--device", default="", help="Target ADB device ID")
    args = parser.parse_args()

    adb_prefix = f"adb -s {args.device}" if args.device else "adb"

    print("=" * 60)
    print("      UE Mobile Inspector - One-Click ADB Injector       ")
    print("=" * 60)

    # 1. Check ADB Devices
    devices_out, _ = run_cmd("adb devices")
    lines = [l for l in devices_out.splitlines()[1:] if l.strip() and "device" in l]
    if not lines:
        print("[-] No ADB device connected! Please connect your Android device with USB debugging enabled.")
        sys.exit(1)
    
    print(f"[+] Found {len(lines)} connected Android device(s).")

    # 2. Check Root Access
    root_check, _ = run_cmd(f"{adb_prefix} shell su -c 'id'")
    if "uid=0" not in root_check:
        print("[-] Root access (su) not available or rejected on the device!")
        print("[!] Note: For non-root devices, please use JSHook / Virtual space / Smali re-packaging.")
        sys.exit(1)
    print("[+] Root (su) permissions verified!")

    # 3. Disable SELinux temporarily to allow ptrace injection
    run_cmd(f"{adb_prefix} shell su -c 'setenforce 0'")
    print("[+] Permissive SELinux enabled (setenforce 0).")

    # 4. Check & Push Binaries
    so_path = args.so
    if not so_path:
        for candidate in ["libUEMobileInspector.so", "release_assets/libUEMobileInspector-arm64.so", "../libUEMobileInspector.so"]:
            if os.path.exists(candidate):
                so_path = candidate
                break

    injector_path = args.injector
    if not injector_path:
        for candidate in ["ue_injector", "release_assets/ue_injector-arm64", "../ue_injector"]:
            if os.path.exists(candidate):
                injector_path = candidate
                break

    if so_path and os.path.exists(so_path):
        print(f"[*] Pushing {so_path} to {REMOTE_TMP}/libUEMobileInspector.so ...")
        run_cmd(f"{adb_prefix} push {so_path} {REMOTE_TMP}/libUEMobileInspector.so")
    
    if injector_path and os.path.exists(injector_path):
        print(f"[*] Pushing {injector_path} to {REMOTE_TMP}/ue_injector ...")
        run_cmd(f"{adb_prefix} push {injector_path} {REMOTE_TMP}/ue_injector")

    # 5. Fix permissions
    run_cmd(f"{adb_prefix} shell su -c 'chmod 777 {REMOTE_TMP}/libUEMobileInspector.so {REMOTE_TMP}/ue_injector'")

    # 6. Find Target Process PID
    package = args.package
    print(f"[*] Searching for running process: {package} ...")
    pid_out, _ = run_cmd(f"{adb_prefix} shell su -c 'pidof {package}'")
    
    if not pid_out.strip():
        print(f"[!] Package '{package}' is not running! Launching app via monkey...")
        run_cmd(f"{adb_prefix} shell monkey -p {package} -c android.intent.category.LAUNCHER 1")
        time.sleep(3)
        pid_out, _ = run_cmd(f"{adb_prefix} shell su -c 'pidof {package}'")

    if not pid_out.strip():
        print(f"[-] Could not find PID for '{package}'. Please open the game manually first.")
        sys.exit(1)

    pid = pid_out.split()[0]
    print(f"[+] Target Process PID: {pid}")

    # 7. Execute Native Injection
    print(f"[*] Executing injection into PID {pid} ...")
    inject_out, code = run_cmd(f"{adb_prefix} shell su -c '{REMOTE_TMP}/ue_injector -pid {pid} -s {REMOTE_TMP}/libUEMobileInspector.so'")
    print(inject_out)

    if "SUCCESS" in inject_out or "completed successfully" in inject_out:
        print("\n" + "=" * 60)
        print(">>> INJECTION SUCCESSFUL! <<<")
        print("Look at your phone screen: the [UE] floating icon should appear.")
        print("=" * 60)
    else:
        print("\n[!] Injection attempt finished. Streaming logcat for verification...")

    # 8. Start Logcat output
    print("[*] Streaming logcat (Press Ctrl+C to stop)...")
    try:
        subprocess.run(f"{adb_prefix} logcat -s UE-Mobile-Inspector", shell=True)
    except KeyboardInterrupt:
        print("\n[*] Detached.")

if __name__ == "__main__":
    main()
