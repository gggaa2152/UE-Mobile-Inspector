#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
UE Mobile Inspector - Automated Zero-Input ADB Injector (/data/1/)
"""

import sys
import os
import subprocess
import time

REMOTE_DIR = "/data/1"

def run_cmd(cmd, check=True):
    res = subprocess.run(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if check and res.returncode != 0:
        print(f"[-] Command failed: {cmd}\n[Error] {res.stderr.strip()}")
    return res.stdout.strip(), res.returncode

def main():
    print("=" * 60)
    print("      UE Mobile Inspector - 零输入自动注入器 (/data/1/)       ")
    print("=" * 60)

    # 1. Check ADB Devices
    devices_out, _ = run_cmd("adb devices")
    lines = [l for l in devices_out.splitlines()[1:] if l.strip() and "device" in l]
    if not lines:
        print("[-] No ADB device connected! Please connect your phone with USB debugging enabled.")
        sys.exit(1)
    
    print(f"[+] Found {len(lines)} connected Android device(s).")

    # 2. Check Root Access & Permissive SELinux
    run_cmd("adb shell su -c 'setenforce 0'")
    run_cmd(f"adb shell su -c 'mkdir -p {REMOTE_DIR}'")
    print(f"[+] Created remote working directory: {REMOTE_DIR}")

    # 3. Push Binaries
    for candidate_so in ["libUEMobileInspector.so", "UEMobileInspector-arm64/libUEMobileInspector.so", "../libUEMobileInspector.so"]:
        if os.path.exists(candidate_so):
            print(f"[*] Pushing {candidate_so} -> {REMOTE_DIR}/libUEMobileInspector.so ...")
            run_cmd(f"adb push {candidate_so} {REMOTE_DIR}/libUEMobileInspector.so")
            break

    for candidate_inj in ["ue_injector", "UEMobileInspector-arm64/ue_injector", "../ue_injector"]:
        if os.path.exists(candidate_inj):
            print(f"[*] Pushing {candidate_inj} -> {REMOTE_DIR}/ue_injector ...")
            run_cmd(f"adb push {candidate_inj} {REMOTE_DIR}/ue_injector")
            break

    if os.path.exists("tools/run_inject.sh"):
        run_cmd(f"adb push tools/run_inject.sh {REMOTE_DIR}/run.sh")

    run_cmd(f"adb shell su -c 'chmod -R 777 {REMOTE_DIR}'")

    # 4. Run zero-input injection
    print("[*] Executing zero-input injection on device...")
    inject_out, _ = run_cmd(f"adb shell su -c '{REMOTE_DIR}/ue_injector'")
    print(inject_out)

    # 5. Live log stream
    print("\n[*] Streaming live logcat output (Ctrl+C to stop)...")
    try:
        subprocess.run("adb logcat -s UE-Mobile-Inspector", shell=True)
    except KeyboardInterrupt:
        print("\n[*] Exited.")

if __name__ == "__main__":
    main()
