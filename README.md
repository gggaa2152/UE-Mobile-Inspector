# UE-Mobile-Inspector (虚幻引擎手游实时动态对象检查器)

[![Build UE Mobile Inspector](https://github.com/gggaa2152/UE-Mobile-Inspector/actions/workflows/build.yml/badge.svg)](https://github.com/gggaa2152/UE-Mobile-Inspector/actions/workflows/build.yml)
[![Platform](https://img.shields.io/badge/Platform-Android%20%7C%20ARM64%20%7C%20ARMv7-brightgreen.svg)]()
[![Engine](https://img.shields.io/badge/Unreal%20Engine-4.18~4.27%20%7C%205.0~5.5-blue.svg)]()

专门为**虚幻引擎（Unreal Engine 4 & 5）手游**打造的**游戏内悬浮式实时动态对象检查与调试工具**（In-Game Runtime Object Inspector & Debugger）。

---

## 🌟 核心特性 (Features)

1. **类与元数据浏览器 (Class Browser)**
   - 全局扫描并列出游戏中所有的 `UClass` 反射类信息。
   - 提供模糊搜索、通配符匹配和继承关系过滤。
   - **“查找实例 (Find Instances)”**：遍历 `GUObjectArray`，极速提取内存中存活的所有活跃对象（如 `BP_PlayerCharacter_C (0x7F2A0410)`）。
   - **“手动输入 (Inspect Address)”**：支持直接输入 `0x...` 内存十六进制地址进行强转查看。

2. **核心对象检查器 (Dynamic Object Inspector)**
   - **面包屑下钻导航 (Breadcrumbs)**：支持如 `World > GameMode > PlayerState > Inventory > Item` 逐层深入子对象，并随时一键点击返回上一层。
   - **实时自动更新 (`[x] 始终更新`)**：每帧自动刷新内存数值（血量、坐标、布尔开关实时跳动）。
   - **属性就地编辑 (In-Place Edit)**：支持直接在表格中修改 `Bool`、`Int`、`Float`、`Double`、`String`、`Vector`、`Rotator` 等字段。
   - **一键 Dump Object**：将对象的完整结构和当前内存数据导出为 JSON 文件存储于 `/sdcard/`。

3. **函数动态调用与 Tracer 追踪 (ProcessEvent)**
   - 列出类的所有 `UFunction`，支持传入参数动态执行 `ProcessEvent`。
   - **实时 Tracer**：全局监听和拦截游戏内所有蓝图/C++ 函数触发，捕获调用者对象、函数名和入参。

4. **触屏悬浮窗与现代暗黑紫粉色主题 (ImGui Mobile)**
   - 基于 OpenGLES3 / EGL 深度挂钩，支持触摸拖拽、缩放及软键盘输入。
   - 高清适配不同移动设备 DPI，提供悬浮球快速唤醒与最小化。

---

## 📁 目录结构

```
.
├── .github/
│   └── workflows/
│       └── build.yml                 # GitHub Actions 自动编译脚本 (Android NDK)
├── CMakeLists.txt                    # CMake 跨平台/NDK 构建文件
├── README.md                         # 项目说明文档
├── .gitignore
└── src/
    ├── main.cpp                      # 动态库入口 (JNI_OnLoad, EGL Hook 初始化)
    ├── config.hpp                    # 全局配置、主题配色
    ├── core/
    │   ├── Memory.hpp / .cpp         # 内存特征码扫描 (AOB Scanner), 内存安全读写
    │   ├── UECore.hpp / .cpp         # 虚幻核心反射 (GUObjectArray, FNamePool/GNames, UObject, UClass, FProperty, UFunction)
    │   ├── UEPropertyReader.hpp/.cpp # 递归属性读写器 (Bool/Int/Float/Vector/String/ObjectPtr/Array/Struct)
    │   └── ProcessEventHook.hpp/.cpp # ProcessEvent 动态调用与实时调用追踪器 (Tracer)
    ├── gui/
    │   ├── GUI.hpp / .cpp            # ImGui 渲染循环主控制器
    │   ├── Style.hpp                 # 1:1 复刻的高级暗黑紫粉色主题
    │   └── views/
    │       ├── ClassBrowserView.hpp/.cpp # 类元数据浏览器与实时实例搜索
    │       ├── ObjectInspectorView.hpp/.cpp # 核心对象检查器 (面包屑导航/实时刷新/属性就地编辑/Dump)
    │       ├── TracerView.hpp/.cpp   # 函数调用追踪器 (Trace 管理与过滤)
    │       ├── ToolsView.hpp/.cpp    # 全局对象转储与虚幻控制台命令执行
    │       └── SettingsView.hpp/.cpp # 触屏灵敏度/透明度/缩放设置
    ├── hook/
    │   ├── EGLHook.hpp / .cpp        # EGL eglSwapBuffers / GLES3 移动端悬浮窗 Hook
    │   └── TouchHook.hpp / .cpp      # Android 触摸事件拦截与 ImGui 交互映射
    └── thirdparty/
        └── dobby/                    # Dobby Hook 头文件
```

---

## 🚀 如何推送到你的 GitHub 仓库

你可以使用以下命令直接将当前本地代码推送到你的 GitHub 仓库：

```bash
# 1. 在当前项目根目录下初始化 Git (如果尚未初始化)
git init
git add .
git commit -m "feat: Initial commit for UE-Mobile-Inspector"

# 2. 关联你的 GitHub 远程仓库 (以 UE-Mobile-Inspector 为例)
git remote add origin https://github.com/gggaa2152/UE-Mobile-Inspector.git
git branch -M main

# 3. 推送到 GitHub (触发 GitHub Actions 自动编译)
git push -u origin main
```

推送成功后，GitHub Actions 会自动拉取 Android NDK r25c 编译器，自动编译出：
- `libUEMobileInspector-arm64-v8a.so`
- `libUEMobileInspector-armeabi-v7a.so`

并在 Release 页面自动打包供你下载！

---

## 📱 使用方式 (Android)

1. 将编译生成的 `libUEMobileInspector.so` 放置到手机中（例如 `/data/local/tmp/`）。
2. 使用注入器（如 `Zygisk` 模块、`Frida`、`Xposed` 或 root 命令行注入器）将 `.so` 注入到目标虚幻手游进程（如 `com.epicgames.portal`、`com.tencent.tmgp.pubgm` 等）。
3. 游戏启动后，屏幕左上方会出现 `UE` 悬浮按钮，点击即可展开完整的实时对象检查器！
