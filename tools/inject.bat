@echo off
chcp 65001 >nul
echo ======================================================
echo       UE Mobile Inspector - 零输入一键注入器 (/data/1/)
echo ======================================================
echo.

echo [*] 正在连接设备并临时放行 SELinux...
adb shell "su -c 'setenforce 0'"

echo [*] 创建 /data/1/ 运行目录并推送全部文件...
adb shell "su -c 'mkdir -p /data/1'"
adb push libUEMobileInspector.so /data/1/libUEMobileInspector.so
adb push ue_injector /data/1/ue_injector
adb push tools/run_inject.sh /data/1/run.sh
adb shell "su -c 'chmod -R 777 /data/1'"

echo [*] 自动扫描并开始注入...
adb shell "su -c '/data/1/ue_injector'"

echo.
echo ======================================================
echo [*] 注入完成！正在监听游戏内悬浮窗日志 (按 Ctrl+C 退出)...
echo ======================================================
adb logcat -s UE-Mobile-Inspector
pause
