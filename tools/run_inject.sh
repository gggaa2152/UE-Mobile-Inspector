#!/system/bin/sh
# UE Mobile Inspector - 手机端一键运行脚本 (目录: /data/1/)

echo "===================================================="
echo " UE Mobile Inspector 自动化注入脚本 (/data/1/)"
echo "===================================================="

# 1. 确保在 /data/1/ 目录下
cd /data/1 || exit 1

# 2. 关闭 SELinux 限制
setenforce 0 2>/dev/null

# 3. 赋予执行权限
chmod -R 777 /data/1

# 4. 执行注入（自动探测游戏进程与 SO 库，无需任何输入）
./ue_injector

echo "===================================================="
