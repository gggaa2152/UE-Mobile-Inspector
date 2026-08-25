@echo off
chcp 65001 >nul
echo ======================================================
echo       UE Mobile Inspector - Windows 一键 ADB 注入器
echo ======================================================
echo.

set TARGET_PACKAGE=%1
if "%TARGET_PACKAGE%"=="" (
    set TARGET_PACKAGE=com.tencent.tmgp.dfm
)

echo [*] 目标游戏包名: %TARGET_PACKAGE%
echo [*] 正在检测已连接的 ADB 设备...
adb devices
echo.

echo [*] 关闭 SELinux 限制 (Permissive 模式)...
adb shell "su -c 'setenforce 0'"

echo [*] 推送注入文件到设备 /data/local/tmp/ ...
adb push libUEMobileInspector.so /data/local/tmp/libUEMobileInspector.so
adb push ue_injector /data/local/tmp/ue_injector
adb shell "su -c 'chmod 777 /data/local/tmp/libUEMobileInspector.so /data/local/tmp/ue_injector'"

echo [*] 正在查找目标进程 PID...
for /f "tokens=1" %%i in ('adb shell "su -c 'pidof %TARGET_PACKAGE%'"') do set TARGET_PID=%%i

if "%TARGET_PID%"=="" (
    echo [!] 游戏未运行，正在尝试启动游戏...
    adb shell monkey -p %TARGET_PACKAGE% -c android.intent.category.LAUNCHER 1
    timeout /t 3 >nul
    for /f "tokens=1" %%i in ('adb shell "su -c 'pidof %TARGET_PACKAGE%'"') do set TARGET_PID=%%i
)

if "%TARGET_PID%"=="" (
    echo [-] 未能获取到游戏 PID，请确保游戏已在手机上打开运行！
    pause
    exit /b 1
)

echo [+] 目标 PID: %TARGET_PID%
echo [*] 正在执行 PTrace 注入...
adb shell "su -c '/data/local/tmp/ue_injector -pid %TARGET_PID% -s /data/local/tmp/libUEMobileInspector.so'"

echo.
echo ======================================================
echo [*] 注入指令已执行完毕，正在监听实时日志 (按 Ctrl+C 退出)...
echo ======================================================
adb logcat -s UE-Mobile-Inspector
pause
