#pragma once

namespace GUI {
    inline const char* GetInspectorHtml() {
        return R"rawhtml(<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>[SV] UE Mobile Inspector</title>
    <style>
        :root {
            --bg-dark: #120914;
            --bg-window: rgba(24, 14, 28, 0.96);
            --bg-child: rgba(36, 20, 42, 0.90);
            --bg-input: rgba(48, 25, 56, 0.85);
            --border-color: rgba(180, 50, 130, 0.45);
            --border-active: #e91e63;
            --primary: #c2185b;
            --primary-hover: #e91e63;
            --primary-active: #ad1457;
            --accent-cyan: #00e5ff;
            --accent-green: #00e676;
            --accent-yellow: #ffd600;
            --text-main: #f5f5f7;
            --text-muted: #a38ba8;
            --font-sans: system-ui, -apple-system, sans-serif;
            --font-mono: monospace;
        }

        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
            user-select: none;
            -webkit-user-select: none;
        }

        body {
            background-color: transparent;
            color: var(--text-main);
            font-family: var(--font-sans);
            font-size: 13px;
            overflow: hidden;
            width: 100vw;
            height: 100vh;
        }

        /* Floating UE Button */
        .floating-ball {
            position: absolute;
            top: 70px;
            left: 20px;
            width: 50px;
            height: 50px;
            border-radius: 50%;
            background: linear-gradient(135deg, #e91e63, #880e4f);
            box-shadow: 0 4px 15px rgba(233, 30, 99, 0.5), 0 0 0 2px rgba(255, 255, 255, 0.3);
            display: flex;
            align-items: center;
            justify-content: center;
            font-weight: 700;
            font-size: 16px;
            color: #fff;
            cursor: pointer;
            z-index: 100;
        }

        /* Main Inspector Window */
        .inspector-window {
            position: absolute;
            top: 50%;
            left: 50%;
            transform: translate(-50%, -50%);
            width: 92vw;
            max-width: 1050px;
            height: 86vh;
            max-height: 720px;
            background: var(--bg-window);
            border: 2px solid var(--border-color);
            border-radius: 10px;
            box-shadow: 0 16px 40px rgba(0, 0, 0, 0.85), 0 0 30px rgba(194, 24, 91, 0.3);
            display: flex;
            flex-direction: column;
            z-index: 50;
            overflow: hidden;
            transition: opacity 0.2s ease, transform 0.2s ease;
        }

        .inspector-window.hidden {
            opacity: 0;
            pointer-events: none;
            transform: translate(-50%, -46%) scale(0.95);
        }

        /* Title Bar */
        .title-bar {
            height: 42px;
            background: rgba(45, 18, 52, 0.95);
            border-bottom: 1px solid var(--border-color);
            display: flex;
            align-items: center;
            justify-content: space-between;
            padding: 0 14px;
            font-weight: 600;
            font-size: 13px;
        }

        .title-bar-left {
            display: flex;
            align-items: center;
            gap: 10px;
        }

        .title-badge {
            background: #e91e63;
            color: white;
            font-size: 10px;
            padding: 2px 6px;
            border-radius: 3px;
            font-weight: 700;
        }

        .title-bar-right {
            display: flex;
            align-items: center;
            gap: 8px;
        }

        .window-control-btn {
            width: 26px;
            height: 26px;
            border-radius: 4px;
            background: rgba(255,255,255,0.12);
            border: none;
            color: #fff;
            display: flex;
            align-items: center;
            justify-content: center;
            cursor: pointer;
            font-size: 14px;
        }

        .window-control-btn:hover {
            background: #e91e63;
        }

        /* Top Nav Tabs */
        .nav-tabs {
            display: flex;
            background: rgba(28, 14, 34, 0.98);
            border-bottom: 1px solid var(--border-color);
            padding: 4px 8px 0;
            gap: 4px;
            overflow-x: auto;
        }

        .nav-tab {
            padding: 8px 16px;
            background: rgba(50, 24, 60, 0.4);
            border: 1px solid transparent;
            border-bottom: none;
            border-radius: 5px 5px 0 0;
            color: var(--text-muted);
            font-weight: 500;
            cursor: pointer;
            white-space: nowrap;
        }

        .nav-tab.active {
            color: #fff;
            background: var(--primary);
            border-color: rgba(255,255,255,0.2);
            box-shadow: 0 -2px 8px rgba(233, 30, 99, 0.4);
        }

        /* Content Views */
        .tab-content-container {
            flex: 1;
            padding: 10px;
            overflow: hidden;
            display: flex;
            flex-direction: column;
        }

        .tab-view {
            display: none;
            width: 100%;
            height: 100%;
            flex-direction: column;
        }

        .tab-view.active {
            display: flex;
        }

        /* Buttons & Inputs */
        button.btn {
            background: var(--primary);
            color: #fff;
            border: none;
            border-radius: 4px;
            padding: 7px 14px;
            font-family: inherit;
            font-size: 12px;
            font-weight: 500;
            cursor: pointer;
            display: inline-flex;
            align-items: center;
            justify-content: center;
            gap: 6px;
        }

        button.btn-secondary {
            background: rgba(255,255,255,0.12);
            color: #ddd;
        }

        input[type="text"], input[type="number"] {
            background: var(--bg-input);
            border: 1px solid var(--border-color);
            color: #fff;
            padding: 7px 10px;
            border-radius: 4px;
            font-family: var(--font-mono);
            font-size: 12px;
            outline: none;
        }

        input[type="text"]:focus {
            border-color: var(--border-active);
        }

        .checkbox-label {
            display: inline-flex;
            align-items: center;
            gap: 6px;
            cursor: pointer;
            color: var(--text-muted);
            font-size: 12px;
        }

        /* Browser Grid */
        .browser-top-bar {
            background: var(--bg-child);
            border: 1px solid var(--border-color);
            border-radius: 6px;
            padding: 8px 12px;
            display: flex;
            flex-direction: column;
            gap: 8px;
            margin-bottom: 8px;
        }

        .browser-search-row {
            display: flex;
            gap: 10px;
        }

        .browser-main-grid {
            flex: 1;
            display: grid;
            grid-template-columns: 280px 1fr;
            gap: 10px;
            overflow: hidden;
        }

        .panel-box {
            background: var(--bg-child);
            border: 1px solid var(--border-color);
            border-radius: 6px;
            padding: 10px;
            display: flex;
            flex-direction: column;
            overflow: hidden;
        }

        .panel-header {
            font-weight: 600;
            color: #ff80ab;
            margin-bottom: 8px;
            padding-bottom: 6px;
            border-bottom: 1px solid rgba(255,255,255,0.08);
            display: flex;
            justify-content: space-between;
            align-items: center;
        }

        .class-list {
            flex: 1;
            overflow-y: auto;
            display: flex;
            flex-direction: column;
            gap: 3px;
        }

        .class-item {
            padding: 7px 10px;
            border-radius: 4px;
            cursor: pointer;
            color: #ccc;
            font-family: var(--font-mono);
            font-size: 12px;
        }

        .class-item.active {
            background: var(--primary);
            color: #fff;
            font-weight: 600;
        }

        .instances-container {
            flex: 1;
            overflow-y: auto;
            display: flex;
            flex-direction: column;
            gap: 6px;
        }

        .instance-card {
            background: rgba(255,255,255,0.05);
            border: 1px solid rgba(255,255,255,0.1);
            border-radius: 5px;
            padding: 8px 12px;
            cursor: pointer;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }

        .instance-card:hover {
            background: rgba(233, 30, 99, 0.3);
            border-color: var(--border-active);
        }

        /* Inspector Tab */
        .breadcrumbs-bar {
            background: var(--bg-child);
            border: 1px solid var(--border-color);
            border-radius: 6px;
            padding: 8px 12px;
            display: flex;
            align-items: center;
            flex-wrap: wrap;
            gap: 6px;
            margin-bottom: 8px;
            font-size: 12px;
        }

        .breadcrumb-chip {
            background: rgba(233, 30, 99, 0.25);
            border: 1px solid rgba(233, 30, 99, 0.5);
            padding: 3px 8px;
            border-radius: 4px;
            color: #ff80ab;
            cursor: pointer;
        }

        .breadcrumb-chip.active {
            background: var(--primary);
            color: #fff;
            font-weight: 600;
        }

        .table-container {
            flex: 1;
            background: var(--bg-child);
            border: 1px solid var(--border-color);
            border-radius: 6px;
            overflow-y: auto;
        }

        table.property-table {
            width: 100%;
            border-collapse: collapse;
            font-size: 12px;
        }

        table.property-table th {
            position: sticky;
            top: 0;
            background: #25102a;
            color: #ff80ab;
            text-align: left;
            padding: 8px 12px;
            border-bottom: 1px solid var(--border-color);
            font-weight: 600;
            z-index: 2;
        }

        table.property-table td {
            padding: 7px 12px;
            border-bottom: 1px solid rgba(255,255,255,0.04);
        }

        .val-editable {
            font-family: var(--font-mono);
            color: var(--accent-cyan);
            cursor: pointer;
            padding: 2px 6px;
            border-radius: 3px;
            background: rgba(0,0,0,0.4);
            display: inline-block;
        }

        .val-bool-true { color: var(--accent-green); font-weight: 600; }
        .val-bool-false { color: #f44336; font-weight: 600; }
        .val-subobject { color: #ff4081; text-decoration: underline; cursor: pointer; }

        /* Tracer Tab */
        .tracer-log-view {
            flex: 1;
            background: #0d060f;
            border: 1px solid var(--border-color);
            border-radius: 6px;
            padding: 8px;
            overflow-y: auto;
            font-family: var(--font-mono);
            font-size: 11px;
            display: flex;
            flex-direction: column;
            gap: 4px;
        }

        .trace-item {
            padding: 4px 8px;
            border-radius: 3px;
            background: rgba(255,255,255,0.03);
            display: flex;
            gap: 10px;
            align-items: center;
        }

        /* Toast */
        .toast {
            position: fixed;
            bottom: 25px;
            right: 25px;
            background: linear-gradient(135deg, #ad1457, #6a1b9a);
            color: #fff;
            padding: 10px 18px;
            border-radius: 6px;
            box-shadow: 0 6px 20px rgba(0,0,0,0.6);
            font-weight: 500;
            z-index: 1000;
            transform: translateY(100px);
            opacity: 0;
            transition: all 0.3s;
        }

        .toast.show {
            transform: translateY(0);
            opacity: 1;
        }
    </style>
</head>
<body>

    <!-- Floating UE Button -->
    <div class="floating-ball" id="floating-btn">UE</div>

    <!-- Main Inspector Window -->
    <div class="inspector-window" id="inspector-window">
        <!-- Title Bar -->
        <div class="title-bar">
            <div class="title-bar-left">
                <span class="title-badge">SV</span>
                <span id="header-title">UE Mobile Inspector v1.0.0 (Delta Force)</span>
            </div>
            <div class="title-bar-right">
                <span id="header-objects-count" style="font-size:11px; color:#a38ba8; font-family:var(--font-mono)">Scanning Engine...</span>
                <button class="window-control-btn" id="btn-minimize" title="最小化">−</button>
            </div>
        </div>

        <!-- Top Navigation Tabs -->
        <div class="nav-tabs">
            <div class="nav-tab active" data-tab="browser">🔍 浏览器 (Browser)</div>
            <div class="nav-tab" data-tab="inspector">🎯 检查器 (Inspector)</div>
            <div class="nav-tab" data-tab="tracer">⚡ 追踪 (Tracer)</div>
            <div class="nav-tab" data-tab="tools">🛠️ 工具 (Tools)</div>
            <div class="nav-tab" data-tab="settings">⚙️ 设置 (Settings)</div>
        </div>

        <!-- Content Area -->
        <div class="tab-content-container">
            
            <!-- 1. Browser Tab -->
            <div class="tab-view active" id="view-browser">
                <div class="browser-top-bar">
                    <div class="browser-search-row">
                        <input type="text" id="class-search-input" placeholder="输入类名搜索 (例如: Player, Character, Controller, Inventory)..." style="flex:1">
                        <button class="btn" id="btn-refresh-classes">🔍 扫描/刷新</button>
                    </div>
                </div>

                <div class="browser-main-grid">
                    <!-- Left: Class List -->
                    <div class="panel-box">
                        <div class="panel-header">
                            <span>类元数据 (Classes)</span>
                            <span id="class-count-label" style="font-size:11px; color:#aaa">0 个匹配</span>
                        </div>
                        <div class="class-list" id="class-list-container"></div>
                    </div>

                    <!-- Right: Class Details & Instances -->
                    <div class="panel-box">
                        <div class="panel-header">
                            <span id="selected-class-title">选中类: 尚未选择</span>
                            <span id="selected-class-super" style="font-size:11px; color:#bbb"></span>
                        </div>
                        <div style="display:flex; gap:10px; margin-bottom:10px; align-items:center">
                            <button class="btn" id="btn-find-instances">⚡ 查找存活实例</button>
                            <input type="text" id="manual-addr-input" placeholder="0x..." style="width:140px">
                            <button class="btn btn-secondary" id="btn-inspect-addr">强制查看</button>
                        </div>
                        <div style="font-weight:600; color:#ddd; margin-bottom:6px; font-size:12px">当前内存活对象 (Active Instances):</div>
                        <div class="instances-container" id="instances-list-container"></div>
                    </div>
                </div>
            </div>

            <!-- 2. Dynamic Object Inspector Tab -->
            <div class="tab-view" id="view-inspector">
                <!-- Breadcrumbs -->
                <div class="breadcrumbs-bar" id="breadcrumbs-container">
                    <span style="color:#aaa">当前路径: </span>
                    <div class="breadcrumb-chip active" id="current-obj-chip">None</div>
                </div>

                <!-- Inspector Controls -->
                <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:8px">
                    <div style="display:flex; gap:12px; align-items:center">
                        <label class="checkbox-label"><input type="checkbox" id="chk-auto-refresh" checked> <b>始终更新</b></label>
                    </div>
                    <input type="text" id="prop-filter-input" placeholder="筛选字段/属性..." style="width:200px">
                </div>

                <!-- Properties Grid -->
                <div class="table-container">
                    <table class="property-table">
                        <thead>
                            <tr>
                                <th style="width:35%">字段 / 属性 (Field)</th>
                                <th style="width:45%">实时值 (Value - 点击可修改)</th>
                                <th style="width:12%">类型 (Type)</th>
                                <th style="width:8%">偏移 (Offset)</th>
                            </tr>
                        </thead>
                        <tbody id="property-table-body"></tbody>
                    </table>
                </div>

                <!-- Bottom Actions -->
                <div style="margin-top:8px; display:flex; justify-content:space-between; align-items:center">
                    <button class="btn" id="btn-dump-object">💾 Dump Object</button>
                    <span id="inspector-status-text" style="color:var(--accent-green); font-size:12px"></span>
                </div>
            </div>

            <!-- 3. Tracer Tab -->
            <div class="tab-view" id="view-tracer">
                <div style="display:flex; justify-content:space-between; margin-bottom:8px">
                    <div style="display:flex; gap:10px">
                        <button class="btn" id="btn-toggle-trace">⏸️ 暂停追踪</button>
                        <button class="btn btn-secondary" id="btn-clear-trace">清空记录</button>
                    </div>
                    <input type="text" id="trace-filter-input" placeholder="过滤函数..." style="width:200px">
                </div>
                <div class="tracer-log-view" id="tracer-log-container"></div>
            </div>

            <!-- 4. Tools Tab -->
            <div class="tab-view" id="view-tools">
                <div class="panel-box" style="margin-bottom:10px">
                    <div class="panel-header">全量 SDK 与对象导出 (Full SDK Dumper)</div>
                    <p style="color:#aaa; margin-bottom:10px">一键扫描 GUObjectArray，将所有类结构体与字段定义导出为 C++ 头文件。</p>
                    <button class="btn" id="btn-full-dump" style="width:260px">🚀 导出全量 SDK 到 /sdcard/</button>
                </div>
                <div class="panel-box">
                    <div class="panel-header">虚幻控制台命令 (Unreal Console)</div>
                    <div style="display:flex; gap:10px">
                        <input type="text" id="console-cmd-input" placeholder="输入控制台命令 (例如: stat fps, slomo 1.5)..." style="flex:1">
                        <button class="btn" id="btn-run-console">执行 (Execute)</button>
                    </div>
                    <div id="console-output" style="margin-top:10px; font-family:var(--font-mono); font-size:11px; color:#bbb; background:#000; padding:8px; border-radius:4px; min-height:80px">
                        > UE Console Ready.
                    </div>
                </div>
            </div>

            <!-- 5. Settings Tab -->
            <div class="tab-view" id="view-settings">
                <div class="panel-box">
                    <div class="panel-header">偏好设置 (Preferences)</div>
                    <div style="display:flex; flex-direction:column; gap:12px; max-width:400px">
                        <div>
                            <div style="display:flex; justify-content:space-between; margin-bottom:4px">
                                <span>触屏缩放:</span>
                                <span id="lbl-scale">1.0x</span>
                            </div>
                            <input type="range" min="0.7" max="1.4" step="0.05" value="1.0" id="rng-scale" style="width:100%">
                        </div>
                    </div>
                </div>
            </div>

        </div>
    </div>

    <!-- Toast Notification -->
    <div class="toast" id="toast-msg"><span>✅ 操作成功</span></div>

    <script>
        const isNative = (typeof window.nativeAPI !== 'undefined');
        let currentClasses = [
            { name: "BP_PlayerCharacter_C", super: "ACharacter" },
            { name: "UCharacterMovementComponent", super: "UActorComponent" },
            { name: "APlayerController", super: "AController" },
            { name: "UWorld", super: "UObject" },
            { name: "UGameEngine", super: "UEngine" }
        ];

        let currentClass = currentClasses[0];
        let currentObjectAddr = "0x70eca6f000";
        let activeProps = {
            "Health": { val: 100.0, type: "FloatProperty", offset: "0x02E8", editable: true },
            "MaxHealth": { val: 100.0, type: "FloatProperty", offset: "0x02EC", editable: true },
            "bIsInvincible": { val: false, type: "BoolProperty", offset: "0x02F0", editable: true },
            "CharacterMovement": { val: "Movement_0 (0x7F2A0880)", type: "ObjectProperty", offset: "0x0310", isSubObj: true }
        };

        const inspectorWindow = document.getElementById("inspector-window");
        const navTabs = document.querySelectorAll(".nav-tab");
        const tabViews = document.querySelectorAll(".tab-view");
        const classListContainer = document.getElementById("class-list-container");
        const instancesContainer = document.getElementById("instances-list-container");
        const propertyTableBody = document.getElementById("property-table-body");
        const tracerLogContainer = document.getElementById("tracer-log-container");

        // Minimize Button
        document.getElementById("btn-minimize").addEventListener("click", () => {
            if (isNative && window.nativeAPI.minimizeWindow) {
                window.nativeAPI.minimizeWindow();
            } else {
                inspectorWindow.classList.add("hidden");
            }
        });

        // Floating Button
        document.getElementById("floating-btn").addEventListener("click", () => {
            if (isNative && window.nativeAPI.showWindow) {
                window.nativeAPI.showWindow();
            } else {
                inspectorWindow.classList.remove("hidden");
            }
        });

        // Tab Switching
        navTabs.forEach(tab => {
            tab.addEventListener("click", () => {
                const targetTab = tab.dataset.tab;
                navTabs.forEach(t => t.classList.remove("active"));
                tabViews.forEach(v => v.classList.remove("active"));
                tab.classList.add("active");
                document.getElementById(`view-${targetTab}`).classList.add("active");
            });
        });

        function switchTab(tabName) {
            navTabs.forEach(t => t.classList.toggle("active", t.dataset.tab === tabName));
            tabViews.forEach(v => v.classList.toggle("active", v.id === `view-${tabName}`));
        }

        // Live Engine Info Sync
        function syncEngineInfo() {
            if (isNative && window.nativeAPI.getEngineInfo) {
                try {
                    const info = JSON.parse(window.nativeAPI.getEngineInfo());
                    if (info.objectsCount !== undefined) {
                        document.getElementById("header-objects-count").innerText = `Objects: ${info.objectsCount.toLocaleString()}`;
                    }
                    if (info.baseAddr) {
                        document.getElementById("header-title").innerText = `⚡ ${info.moduleName} (0x${info.baseAddr})`;
                    }
                } catch(e) {}
            }
        }
        setInterval(syncEngineInfo, 1000);
        syncEngineInfo();

        // Render Class List
        function renderClassList(filter = "") {
            classListContainer.innerHTML = "";
            if (isNative && window.nativeAPI.getClassesList) {
                try {
                    const res = JSON.parse(window.nativeAPI.getClassesList(filter));
                    if (Array.isArray(res) && res.length > 0) {
                        currentClasses = res;
                    }
                } catch(e) {}
            }

            const filtered = currentClasses.filter(c => c.name.toLowerCase().includes(filter.toLowerCase()));
            document.getElementById("class-count-label").innerText = `${filtered.length} 个匹配`;

            filtered.forEach(c => {
                const item = document.createElement("div");
                item.className = `class-item ${c.name === (currentClass ? currentClass.name : '') ? 'active' : ''}`;
                item.innerText = c.name;
                item.onclick = () => {
                    currentClass = c;
                    document.querySelectorAll(".class-item").forEach(el => el.classList.remove("active"));
                    item.classList.add("active");
                    document.getElementById("selected-class-title").innerText = `选中类: ${c.name}`;
                    document.getElementById("selected-class-super").innerText = c.super ? `Super: ${c.super}` : "";
                    renderInstances();
                };
                classListContainer.appendChild(item);
            });
        }

        // Render Instances
        function renderInstances() {
            instancesContainer.innerHTML = "";
            let instances = [
                { name: `${currentClass ? currentClass.name : 'Object'}_0`, addr: "0x7F2A0410" },
                { name: `${currentClass ? currentClass.name : 'Object'}_1`, addr: "0x7F2A1280" }
            ];

            if (isNative && window.nativeAPI.getInstancesList && currentClass) {
                try {
                    const res = JSON.parse(window.nativeAPI.getInstancesList(currentClass.name));
                    if (Array.isArray(res) && res.length > 0) instances = res;
                } catch(e) {}
            }

            instances.forEach(inst => {
                const card = document.createElement("div");
                card.className = "instance-card";
                card.innerHTML = `
                    <div>
                        <div style="font-weight:600; color:#fff">${inst.name}</div>
                        <div style="font-size:11px; color:#aaa">${currentClass ? currentClass.name : ''}</div>
                    </div>
                    <div style="font-family:var(--font-mono); color:var(--accent-cyan); font-size:11px">${inst.addr}</div>
                `;
                card.onclick = () => {
                    inspectObject(inst.name, inst.addr);
                };
                instancesContainer.appendChild(card);
            });
        }

        function inspectObject(name, addr) {
            currentObjectAddr = addr;
            document.getElementById("current-obj-chip").innerText = `${name} (${addr})`;
            if (isNative && window.nativeAPI.inspectObject) {
                try {
                    const res = JSON.parse(window.nativeAPI.inspectObject(addr));
                    if (res && Object.keys(res).length > 0) activeProps = res;
                } catch(e) {}
            }
            renderProperties();
            switchTab("inspector");
            showToast(`已进入对象: ${name}`);
        }

        function renderProperties() {
            const filter = document.getElementById("prop-filter-input").value.toLowerCase();
            propertyTableBody.innerHTML = "";

            for (const [key, info] of Object.entries(activeProps)) {
                if (filter && !key.toLowerCase().includes(filter)) continue;

                const tr = document.createElement("tr");
                let valHtml = "";
                if (info.type === "BoolProperty") {
                    valHtml = `<span class="val-editable ${info.val ? 'val-bool-true' : 'val-bool-false'}" onclick="toggleBool('${key}')">${info.val ? 'True (开启)' : 'False (关闭)'}</span>`;
                } else if (info.isSubObj) {
                    valHtml = `<span class="val-subobject" onclick="inspectObject('${info.val}', '${info.val.split('(')[1]?.replace(')','') || ''}')">👉 ${info.val}</span>`;
                } else if (info.editable) {
                    valHtml = `<span class="val-editable" onclick="editValue('${key}', '${info.offset}', '${info.type}')">${info.val}</span>`;
                } else {
                    valHtml = `<span>${info.val}</span>`;
                }

                tr.innerHTML = `
                    <td style="font-weight:500; color:#f1f1f1">${key}</td>
                    <td>${valHtml}</td>
                    <td style="font-family:var(--font-mono); color:#b39ddb; font-size:11px">${info.type}</td>
                    <td style="font-family:var(--font-mono); color:rgba(255,255,255,0.4); font-size:11px">${info.offset}</td>
                `;
                propertyTableBody.appendChild(tr);
            }
        }

        function toggleBool(propName) {
            activeProps[propName].val = !activeProps[propName].val;
            if (isNative && window.nativeAPI.modifyFieldValue) {
                window.nativeAPI.modifyFieldValue(currentObjectAddr, activeProps[propName].offset, "BoolProperty", activeProps[propName].val ? "1" : "0");
            }
            renderProperties();
            showToast(`${propName} -> ${activeProps[propName].val}`);
        }

        function editValue(propName, offset, type) {
            const current = activeProps[propName].val;
            const res = prompt(`输入新数值 (${propName}):`, current);
            if (res !== null) {
                activeProps[propName].val = parseFloat(res) || res;
                if (isNative && window.nativeAPI.modifyFieldValue) {
                    window.nativeAPI.modifyFieldValue(currentObjectAddr, offset, type, res);
                }
                renderProperties();
                showToast(`写入成功: ${res}`);
            }
        }

        document.getElementById("btn-refresh-classes").addEventListener("click", () => {
            renderClassList(document.getElementById("class-search-input").value);
            showToast("类元数据已刷新");
        });

        document.getElementById("class-search-input").addEventListener("input", (e) => {
            renderClassList(e.target.value);
        });

        document.getElementById("btn-find-instances").addEventListener("click", () => {
            renderInstances();
            showToast("存活实例扫描完成");
        });

        document.getElementById("btn-inspect-addr").addEventListener("click", () => {
            const addr = document.getElementById("manual-addr-input").value.trim();
            if (addr) inspectObject("ManualObject", addr);
        });

        document.getElementById("prop-filter-input").addEventListener("input", renderProperties);

        document.getElementById("btn-dump-object").addEventListener("click", () => {
            showToast("✅ 对象已转储到 /sdcard/UE_Inspector_Dumps/");
        });

        document.getElementById("btn-full-dump").addEventListener("click", () => {
            if (isNative && window.nativeAPI.dumpSDK) {
                const res = window.nativeAPI.dumpSDK();
                showToast(res);
            } else {
                showToast("✅ SDK 已导出至 /sdcard/UE_Inspector_Dumps/Full_SDK.txt");
            }
        });

        document.getElementById("btn-run-console").addEventListener("click", () => {
            const cmd = document.getElementById("console-cmd-input").value;
            if (!cmd) return;
            if (isNative && window.nativeAPI.executeConsoleCmd) {
                window.nativeAPI.executeConsoleCmd(cmd);
            }
            const output = document.getElementById("console-output");
            output.innerHTML += `<br><span style="color:#00e5ff">> ${cmd}</span><br><span style="color:#8bc34a">Command executed.</span>`;
            document.getElementById("console-cmd-input").value = "";
            output.scrollTop = output.scrollHeight;
        });

        document.getElementById("rng-scale").addEventListener("input", (e) => {
            document.getElementById("lbl-scale").innerText = `${e.target.value}x`;
            inspectorWindow.style.transform = `translate(-50%, -50%) scale(${e.target.value})`;
        });

        // Toast Helper
        function showToast(msg) {
            const toast = document.getElementById("toast-msg");
            toast.querySelector("span").innerText = msg;
            toast.classList.add("show");
            setTimeout(() => { toast.classList.remove("show"); }, 2000);
        }

        // Initialize UI
        renderClassList();
        renderInstances();
        renderProperties();
    </script>
</body>
</html>)rawhtml";
    }
}
