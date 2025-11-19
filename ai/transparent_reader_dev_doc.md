# Transparent Markdown Reader – 技术设计文档（章节结构 v1.0）

本文件为项目完整章节化技术文档框架，后续所有技术细节将基于此结构展开。

---

# 目录

1. 项目概述  
2. 架构与模块设计  
3. 透明窗口与无边框技术（已整合 Ctrl 临时解锁 + 自动返焦） 
4. 窗口布局与交互设计（整窗可拖动版）  
5. Markdown 渲染系统（Qt WebEngine + marked.js） 
6. 主题系统设计（文字 / 背景 / 透明度）  
7. 文档状态持久化（历史记录 + 阅读进度））  
8. 单实例机制（QLocalServer + QLocalSocket） 
9. 拖放文件到阅读窗口（dragEnterEvent / dropEvent）  
10. 系统托盘设计（QSystemTrayIcon）  
11. 配置与数据存储结构  
12. 性能优化
13. 可扩展性规划  
14. 工程目录结构规范 

---

# 1. 项目概述

Transparent Markdown Reader 是一个 **仅针对 Windows** 的轻量级透明 Markdown 阅读器。

它的核心定位是：  
> 把一篇或多篇 `.md` 文档“贴”在屏幕上方，以接近全屏透明叠加的方式展示，  
> 保留最基本的交互（翻页、滚动、点击链接），  
> 尽量 **不打断用户当前正在做的事**。

为此，项目围绕以下几个关键点设计：

- **透明窗口 + 锁定模式**：阅读窗口可以像「玻璃」一样漂浮在其它应用上方；锁定后鼠标穿透，不抢焦点。
- **Ctrl 临时解锁**：按住 Ctrl 时临时进入“可操作”模式，松开 Ctrl 自动恢复穿透并把焦点还给原工作窗口。
- **右键翻页**：在锁定状态下，右键点击窗口的上半部分 / 下半部分，分别翻到上一页 / 下一页。
- **Markdown 渲染稳定可控**：使用 Qt WebEngine + HTML + marked.js 渲染 Markdown，所有样式由 CSS 管控。
- **阅读进度记忆**：每个文档的滚动位置、最近打开列表都会持久化，下次打开自动回到上次看到的位置。
- **单实例 + 拖放**：整个系统只允许一个实例运行，拖 `.md` 到 exe / 窗口 / 托盘入口，都会交给同一个主实例处理。
- **系统托盘控制中心**：阅读窗口只保留 `🔒` 和 `X` 两个按钮，其它所有设置（主题、透明度、开机自启、调试日志等）放到托盘菜单。

---

## 1.1 使用场景

典型目标场景：

1. **学习 / 复习技术文档**
   - 一边写代码，一边在屏幕侧边透明叠加一份 C++ / Markdown 教程；
   - 锁定后鼠标左键、滚轮都穿透到 IDE，只有右键用于翻页；
   - 按住 Ctrl 临时操作文档（滚动、点链接），放开 Ctrl 又回到 IDE。

2. **做笔记时参考现有资料**
   - 主屏是 Obsidian / VS Code / Word；
   - Transparent Markdown Reader 透明叠在一侧展示参考 md；
   - 通过内部链接在不同 md 之间跳转，同时记住每篇文档的阅读位置。

3. **复盘 / 复习自己的知识库**
   - 将本地导出的学习笔记（已转成 md）交给阅读器；
   - 通过托盘最近文档列表快速切换；
   - 每次打开都自动回到上次阅读位置。

---

## 1.2 目标用户与非目标

**目标用户：**

- 主要在 Windows 上工作的开发者 / 工程师；
- 大量阅读 Markdown 文档（教程、笔记、规范）；
- 希望阅读窗口不抢焦点、不挡工作流，只在需要时快速翻页、跳转。

**非目标（当前明确不做的事情）：**

- 不做富文本编辑器：本项目只负责阅读，不提供 Markdown 编辑能力。
- 不做云同步 / 多端协作：只读本地文件，不涉及账号、同步、分享。
- 不做跨平台统一体验：当前仅针对 Windows 做到体验优秀，其他平台放在后续扩展规划中。

---

## 1.3 目标平台与技术栈

- **操作系统：**
  - 优先支持 **Windows 10 / Windows 11**；
  - 部分代码使用 Windows API（例如自动返焦、开机自启），其它平台暂不考虑。

- **开发语言与框架：**
  - C++17 / C++20；
  - Qt 6（Widgets + WebEngineWidgets + WebChannel）；
  - 使用 CMake 作为主要构建系统。

- **前端渲染：**
  - `QWebEngineView` + 本地 `index.html`；
  - `marked.js` 负责 Markdown → HTML 渲染；
  - CSS 控制文字颜色、透明度、背景颜色 / 背景图片等主题效果。

---

## 1.4 核心功能一览（与章节对应）

为了给后面章节一个“导航地图”，这里按功能列出主要能力以及在文档中的位置：

1. **透明窗口与输入穿透**（第 3 章）
   - 无边框透明主窗口；
   - 锁定模式下，窗口整体对鼠标透明，仅保留按钮栏可点击；
   - 右键不穿透，用于翻页。

2. **窗口布局与交互**（第 4 章）
   - 简单的顶部按钮栏：`🔒` + `X`；
   - 窗口整体可拖动；
   - 右键上半部分上一页、下半部分下一页。

3. **Markdown 渲染引擎**（第 5 章）
   - Qt WebEngine + HTML + marked.js 渲染 md 文件；
   - 前端负责渲染与样式，C++ 只负责读文件和业务逻辑；
   - 渲染完成后再恢复滚动位置，保证阅读体验。

4. **主题与透明度系统**（第 6 章）
   - 文字颜色、透明度可调；
   - 背景颜色、透明度 / 背景图片可配置；
   - 所有设置通过托盘菜单操作，阅读窗口只负责显示。

5. **历史记录与阅读进度持久化**（第 7 / 8 章）
   - 每个文档记住滚动位置；
   - 最近打开列表；
   - 配置文件与状态持久化（便携版 / 安装版路径）。

6. **链接处理**（第 7 章）
   - 内部链接：从 A.md 跳到 B.md，同时保存 A 的阅读位置；
   - 外部链接：http / https 由系统默认浏览器打开；
   - 鼠标 hover 时有高亮提示，不与 Ctrl 临时解锁冲突。

7. **单实例与文件拖放**（第 9 / 10 章）
   - 使用 `QLocalServer + QLocalSocket` 实现单实例；
   - 已运行实例接收新文件路径，当前实例切换文档；
   - 支持：
     - 拖 `.md` 到 exe；
     - 拖 `.md` 到窗口；
     - 双击 `.md`（文件关联）；
   - 所有入口最终统一走同一套“保存当前进度 → 打开新文档 → 恢复新文档进度”的流程。

8. **系统托盘与开机自启 / 日志**（第 10 章）
   - `QSystemTrayIcon + QMenu` 构建托盘菜单；
   - 控制窗口显示/隐藏、最近打开、主题/透明度、开机自启、调试日志、退出；
   - Windows 下通过注册表实现用户级开机自启；
   - 可打开调试日志开关，方便排查问题。

9. **配置存储与工程结构、扩展规划**（第 14–17 章）
   - 使用单一 JSON 文件管理配置与文档状态；
   - 定义清晰的工程目录结构（src/core/render/platform/...）；
   - 性能优化建议（WebEngine 设置、事件循环等）；
   - 可扩展性规划（搜索、注释、更多渲染模式、未来跨平台等）。

---

## 1.5 名词与约定

为了在后续章节中表述清晰，本项目约定以下名词：

- **阅读窗口 / 主窗口**：指透明的主界面，内含 Markdown 内容和 `🔒` / `X` 按钮。
- **锁定（🔒）**：窗口对鼠标输入透明，仅保留必要的右键翻页和按钮栏交互。
- **未锁定（🔓）**：窗口行为与普通应用一致，可以滚动、选择文字、点击链接。
- **Ctrl 临时解锁**：锁定状态下按住 Ctrl，使窗口暂时进入“未锁定”行为，松手后恢复锁定并自动把焦点还给原前台窗口。
- **主实例**：当前正在运行、负责实际渲染和打开文档的唯一进程。
- **便携版**：配置与日志文件存放在 exe 同目录，方便整个文件夹移动到其它机器使用。

---

## 1.6 本章小结

本章给出了 Transparent Markdown Reader 的整体定位、使用场景和核心功能清单，为后续各章节的技术细节提供“全局地图”。

如果需要快速了解某一块的具体实现，可以从本章的功能列表跳转到对应章节阅读；如果要新增功能或调整需求，也建议先回看本章，确认是否仍然符合“透明、轻量、不打断工作流”的初始设计目标。


# 2. 架构与模块设计

本章从整体角度说明透明 Markdown 阅读器的 **进程结构、分层架构、主要模块职责** 以及几个典型流程的调用关系。
工程目录结构见第 14 章.

目标：

- 一眼看清：这个程序从“进程 → 模块 → 类”是怎么拆的；
- 新增功能时能快速判断：应该改哪一层、加在哪个模块；
- 为后面章节（单实例、托盘、配置、扩展性）提供一个“地图”。

---

## 2.1 总体架构概览

### 2.1.1 进程结构（逻辑上）

- **主进程（Qt Widgets 应用）**
  - 负责窗口、托盘、配置读取/写入、文档状态管理、路由各种入口（拖放、托盘菜单、单实例消息等）。
- **WebEngine 子进程（QtWebEngine 内部）**
  - 负责渲染 HTML / CSS / JS；
  - 通过 `QWebEngineView + QWebChannel` 与主进程通信（如滚动位置回传、链接点击等）。

> 本项目所有“业务逻辑”和“状态”都在主进程，WebEngine 视为一个可替换的渲染黑盒。

### 2.1.2 分层结构（逻辑分层）

从上往下大致分为 5 层：

1. **UI 层（app/ui）**
   - `MainWindow`：主窗口，负责锁定/解锁、拖动、关闭等；
   - `TrayManager` 或托盘相关代码：托盘图标 + 菜单；
   - 简单 UI 控件（标题栏、🔒 按钮等）。

2. **渲染层（render）**
   - `WebViewHost`：封装 `QWebEngineView`，统一加载页面、设置 WebEngine 属性；
   - `MarkdownPage`：自定义 `QWebEnginePage`，拦截链接点击；
   - `Backend`：`QWebChannel` 后端对象，处理从 JS 回调到 C++ 的消息（例如报告当前滚动位置）。

3. **核心业务层（core）**
   - `AppState`：全局应用状态（主题、最近文件、日志开关等）；
   - `DocumentState`：单个文档的阅读位置、最后打开时间等；
   - 文档导航逻辑：`navigateTo(path)`、`openMarkdownFile(path)` 等；
   - 统一“切换文档”流程（保存当前 → 打开新文档 → 恢复新文档位置）。

4. **平台适配层（platform）**
   - Windows 专用逻辑：
     - `SingleInstance_win`：单实例（`QLocalServer + QLocalSocket` 封装）；
     - `AutoStart_win`：开机自启（注册表 `Run` 项）；
     - `FileAssoc_win`：文件关联 / 拖到 exe 的处理（主要是命令行参数解析 + 单实例转发）；
     - `WinApiHelpers`：如需要用到 `SetForegroundWindow` 之类辅助。
   - `Paths`（common）：统一配置、日志目录的路径计算（安装版 / 便携版）。

5. **基础设施层（infra）**
   - `Settings`：负责 `AppState` 的 JSON 读写（基于 `QJsonDocument + QSaveFile`）；
   - `Logger`：`QLoggingCategory` + `qInstallMessageHandler`，实现调试日志与文件日志；
   - 其它小工具类（如路径规范化、时间格式化等）。

---

## 2.2 主要模块与职责

### 2.2.1 MainWindow（主窗口）

**职责：**

- 创建并管理主阅读窗口；
- 持有 `WebViewHost`、`AppState` 等核心对象；
- 处理锁定/解锁、Ctrl 临时解锁、窗口拖动、关闭事件；
- 响应托盘菜单发来的命令（显示/隐藏、打开文件、主题切换等）；
- 连接单实例管理器发来的“打开新文件”信号。

**不做的事：**

- 不直接操作配置文件（交给 `Settings`）；
- 不做复杂状态保存（统一通过 `AppState`）；
- 不写 Windows 特定 API（交给 `platform` 层）。

---

### 2.2.2 TrayManager / 托盘相关

可以是单独的 `TrayManager` 类，也可以直接集成在 `MainWindow` 里，职责包括：

- 创建 `QSystemTrayIcon` 和 `QMenu`；
- 定义托盘菜单结构：
  - 打开文件；
  - 最近打开；
  - 主题 / 透明度预设；
  - 显示/隐藏窗口；
  - 始终置顶；
  - 开机自启；
  - 调试日志 / 写入日志文件；
  - 退出程序；
- 处理托盘点击（单击/双击切换窗口可见，右键显示菜单）。

托盘本身不保存状态，只是把用户操作转成调用：

- `MainWindow::navigateTo(path)`；
- `MainWindow::applyTheme(...)`；
- `MainWindow::setAutoStartEnabled(...)`；
- `MainWindow::setDebugLoggingEnabled(...)` 等。

---

### 2.2.3 WebViewHost / MarkdownPage / Backend（渲染层）

**WebViewHost**

- 封装一个 `QWebEngineView`；
- 在构造时配置 `QWebEngineSettings`：
  - 关闭不必要的功能（插件、全屏、滚动动画等）；
  - 设置本地 html / js / css；
- 提供接口：
  - `loadMarkdownHtml(const QString &html)`；
  - `scrollToY(qint64 y)`；
  - `evaluateJavaScript(...)`。

**MarkdownPage**

- 继承 `QWebEnginePage`；
- 重载 `acceptNavigationRequest`，拦截点击事件：
  - 内部链接（指向 `.md` 或锚点） → 发 signal 给 C++ 主逻辑；
  - 外部链接（http/https） → 用 `QDesktopServices::openUrl()` 打开。

**Backend（QWebChannel 后端）**

- 暴露给 JS 的对象；
- JS 在滚动或跳转时可以调用：
  - `backend.reportScrollPosition(y)`；
  - `backend.requestOpenLink("other.md")` 等；
- C++ 通过连接这些信号更新 `DocumentState` 或发起 `navigateTo()`。

---

### 2.2.4 AppState / DocumentState / Settings（核心状态）

**AppState**

- 保存整个应用的长期状态：
  - 主题名称、透明度；
  - 窗口几何信息、是否置顶、上次是否锁定；
  - 自动启动、调试日志开关、文件日志开关；
  - 最近文档列表 `recentFiles`；
  - 最后打开的文档 `lastOpenedFile`；
  - 每个文档的 `DocumentState`。

**DocumentState**

- 针对单个 `.md` 文档的状态：
  - `lastScrollY`：最后阅读位置；
  - `lastOpened`：最后打开时间；
  - 将来可扩展：`zoom`、`lastHeadingId`、注释等。

**Settings**

- 提供两个核心函数：
  - `bool loadAppState(AppState &out);`
  - `bool saveAppState(const AppState &state);`
- 内部用 JSON + `QSaveFile` 读写配置；
- 负责处理便携模式 / 安装模式的路径差异（通过 `Paths`）。

---

### 2.2.5 SingleInstance / AutoStart / Paths（平台适配层）

**SingleInstance_win**

- 封装 `QLocalServer` 和 `QLocalSocket`：
  - 启动时检查是否已有主实例；
  - 若已有主实例：把命令行传入的文件路径发给主实例，然后当前进程退出；
  - 若没有：当前成为主实例，监听其他进程发送的“打开文件”请求；
- 对外暴露信号：
  - `openFileRequested(const QString &path)`。

**AutoStart_win**

- 负责 Windows 下“开机自启”的实现：
  - 读取/写入 `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` 中自己的键值；
- 对外暴露：
  - `static bool isSupported();`
  - `static bool isEnabled();`
  - `static void setEnabled(bool on);`

**Paths**

- 统一计算配置、日志目录：
  - 如果存在 `portable.flag` → 便携模式：配置/日志在 exe 同目录；
  - 否则使用 `QStandardPaths::AppConfigLocation`；
- 提供：
  - `QString detectConfigDir();`
  - `QString detectConfigPath();`
  - `QString logDirPath();` 等。

---

### 2.2.6 Logger（日志系统）

- 定义 logging category：
  - `Q_LOGGING_CATEGORY(lcReader, "reader");`
- 提供：
  - `void setDebugLoggingEnabled(bool on);` → 调整 `QLoggingCategory::setFilterRules`；
  - `void installFileLogger(bool enabled);` → 使用 `qInstallMessageHandler` 写日志文件；
- 由托盘菜单控制调试/文件日志开关，状态持久化在 `AppState` 中。

---

## 2.3 典型流程调用关系

### 2.3.1 程序启动流程

1. `main()`：
   - 设置应用名/组织名；
   - 解析命令行参数（是否带 `.md` 路径）。
2. SingleInstance：
   - 检查是否已有主实例；
   - 若已有：通过 `QLocalSocket` 把路径发给主实例 → 退出；
   - 若没有：当前进程成为主实例。
3. 创建 `QApplication` → 创建 `MainWindow`：
   - `Settings::loadAppState()` 读取配置；
   - 根据 `AppState` 恢复窗口几何、锁定状态、主题；
   - 初始化 `WebViewHost` / WebEngine。
4. 如果 `AppState.lastOpenedFile` 非空且文件存在：
   - 调用 `navigateTo(lastOpenedFile)` 打开；
   - 渲染完成后恢复 `lastScrollY`。

---

### 2.3.2 打开文档 / 切换文档流程

入口统一是 `MainWindow::navigateTo(const QString &path)`，调用链：

1. 入口可能来自：
   - 托盘“打开文件…”；
   - 托盘“最近打开”；
   - 拖放 `.md` 到窗口；
   - 拖放 `.md` 到 exe；
   - 双击 `.md`（文件关联）；
   - 单实例收到了新文件路径；
   - 内部链接点击（从 A.md 跳到 B.md）。
2. `navigateTo(path)`：
   - `captureScrollPosition()`：把当前文档的滚动位置写回对应 `DocumentState`；
   - 更新 `recentFiles`；
   - 更新 `lastOpenedFile`；
   - 调用 `openMarkdownFile(path)`。
3. `openMarkdownFile(path)`：
   - 通过普通 `QFile` 读取 md；
   - 如需要先在 C++ 层转 HTML，则调用 `IMarkdownRenderer`；
   - 把 HTML 交给 `WebViewHost::loadMarkdownHtml()`。
4. 页面加载完成时（WebEngine 发信号）：
   - 调用前端 JS 注入一些初始化脚本（主题、透明度、链接处理）；
   - 根据 `DocumentState.lastScrollY` 调用 `scrollToY(y)`；
   - 前端可以回调 `Backend` 报告真实滚动位置。

---

### 2.3.3 锁定 / Ctrl 临时解锁 / 返回焦点流程

1. 点击窗口内的 `🔒` 按钮：
   - 切换锁定状态：
     - 锁定：加上 `Qt::WindowTransparentForInput`，只保留按钮栏可点击；
     - 解锁：恢复正常窗口行为。
2. 锁定状态下按住 Ctrl：
   - 通过 `keyPressEvent` 检测；
   - 暂时去掉透明输入 flag，使窗口可以滚动/点击；
   - 结束时（Ctrl 弹起）再次打开透明输入：
     - 把焦点使用 `SetForegroundWindow` 或 Qt 自身 API 还给之前的前台窗口；
     - 保证不会长时间抢占用户当前工作窗口。

---

### 2.3.4 退出流程

1. 用户点击主窗口 `X` 或托盘“退出”；
2. `MainWindow::onActionQuit()`：
   - `captureScrollPosition()`；
   - 调用 `Settings::saveAppState(m_state)`；
   - `qApp->quit()`；
3. Qt 正常销毁对象，进程退出。

---

## 2.4 模块依赖关系约束

为了让项目后期扩展不混乱，本项目约定以下依赖方向：

- **UI 层可以依赖 core/render/platform/infra**；
- **core 可以依赖 infra（Settings / Logger / Paths），但尽量不依赖 UI 或 platform 特定实现**；
- **render 只依赖 core/infra，不反向依赖 UI**；
- **platform 封装平台细节，不依赖 UI；core/Ui 通过抽象接口使用它**。

简单原则：

> 新增功能时，如果发现需要在多个层乱 import / include，优先考虑：  
> “是不是应该抽成一个 core 或 infra 模块，再在不同地方调用？”

---

## 2.5 与后续章节的关系

- 第 3–6 章：在本章的架构基础上，细化窗口、渲染、主题的实现；
- 第 7–10 章：围绕链接、历史、单实例、拖放，具体展开“典型流程”的代码；
- 第 13–15 章：在 Logger / Settings / 目录结构的基础上补充托盘、日志、性能优化；
- 第 17 章：可扩展性规划正是建立在本章的分层与模块边界之上。

本章定义的只是“骨架”，后续所有章节都是在这个骨架上加肉。以后你要新增功能或改设计时，优先考虑它应该属于哪一层、落在哪个模块里，避免把所有东西都堆进 `MainWindow`。




# 3：透明窗口与无边框技术（已整合 Ctrl 临时解锁 + 自动返焦）

## 3.1 透明窗口基础
```cpp
setAttribute(Qt::WA_TranslucentBackground);
setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
```

## 3.2 锁定状态的鼠标穿透（🔒=ON）
在 Windows / X11 上可使用：
```cpp
setWindowFlag(Qt::WindowTransparentForInput, true);
show();
```
> 注意：Wayland 下该 flag 可能无效。

内容区可穿透：
```cpp
webView->setAttribute(Qt::WA_TransparentForMouseEvents, true);
```

按钮栏不可穿透：
```cpp
btnBar->setAttribute(Qt::WA_TransparentForMouseEvents, false);
```

## 3.3 右键不穿透，用于翻页
通过事件过滤器捕获右键：
```cpp
bool MainWindow::eventFilter(QObject *obj, QEvent *e){
    if(lockedMode && e->type() == QEvent::MouseButtonPress){
        auto m = static_cast<QMouseEvent*>(e);
        if(m->button() == Qt::RightButton){
            nextPage();
            return true;
        }
    }
    return QObject::eventFilter(obj, e);
}
```

## 3.4 按住 Ctrl → 临时解锁（临时🔓）
当 Ctrl 按下时：
```cpp
if(k->key() == Qt::Key_Control && lockedMode){
#ifdef Q_OS_WIN
    activeWindowBeforeCtrl = GetForegroundWindow();
#endif
    setWindowFlag(Qt::WindowTransparentForInput, false);
    show();
    webView->setAttribute(Qt::WA_TransparentForMouseEvents, false);
}
```

## 3.5 松开 Ctrl → 恢复穿透 + 自动返还焦点（Windows）
恢复穿透：
```cpp
setWindowFlag(Qt::WindowTransparentForInput, true);
show();
webView->setAttribute(Qt::WA_TransparentForMouseEvents, true);
```
```cpp
void MainWindow::onCtrlReleased()
{
    if (!lockedMode)
        return;

    // 恢复锁定时的穿透设置
    setWindowFlag(Qt::WindowTransparentForInput, true);
    show();  // 让 flag 生效

    webView->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    btnBar->setAttribute(Qt::WA_TransparentForMouseEvents, false);

#ifdef Q_OS_WIN
    if (activeWindowBeforeCtrl) {
        SetForegroundWindow(activeWindowBeforeCtrl);
        activeWindowBeforeCtrl = nullptr;
    }
#endif
}
返还焦点（Windows API）：
```cpp
#ifdef Q_OS_WIN
if(activeWindowBeforeCtrl)
    SetForegroundWindow(activeWindowBeforeCtrl);
#endif
```

## 3.6 交互流程总结
1. 🔒=ON → 全穿透（按钮除外）
2. 右键 → 翻页
3. 按住 Ctrl → 临时可操作
4. 松开 Ctrl → 恢复穿透，并回到原工作窗口（自动返焦）



# 4：窗口布局与交互设计（整窗可拖动版）

本章定义阅读窗口的 **尺寸规划、区域划分、鼠标行为规则**，以及与锁定/解锁、Ctrl 临时解锁的交互关系。  
本版本对拖动行为进行了更新：在允许拖动时，**整个窗口区域都可以按住左键拖动移动窗口**。

---

## 4.1 设计目标

1. **窗口大小**：默认适合长篇 Markdown 阅读，可在未🔒或 Ctrl 临时解锁时调整位置、大小。  
2. **右键翻页区域**：
   - 窗口上半部分：右键 → 上一页
   - 窗口下半部分：右键 → 下一页
3. **锁定 / 未锁定模式行为**：
   - 🔒 开启：窗口只作为“透明阅读层”，除右键翻页外，其余鼠标事件尽量穿透到底层应用。
   - 🔒 关闭：窗口行为类似普通阅读器，可拖动、滚动、选择文本等。
4. **Ctrl 临时解锁**：按住 Ctrl 时，临时进入未🔒行为；松开即恢复 🔒 行为，并自动把焦点还给原工作窗口（第 3 章已定义）。
5. **整窗拖动**：在允许拖动的状态下（未🔒或 Ctrl 临时解锁），**整个窗口区域按住左键即可拖动移动**。

---

## 4.2 窗口尺寸规划

### 4.2.1 默认尺寸

```cpp
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    resize(720, 900);               // 默认大小
    setMinimumSize(480, 600);       // 最小尺寸，避免布局挤压
}
````

### 4.2.2 尺寸调整策略

* 🔒 锁定时：建议不允许调整大小，避免误操作。
* 🔓 未锁定 / Ctrl 临时解锁 时：允许用户调整大小（Qt 默认边缘缩放即可）。
* 若后续需要“自绘边框 + 自定义缩放”，可在后续章节扩展。

---

## 4.3 区域划分与右键翻页

右键翻页逻辑基于点击位置的 y 坐标相对于窗口高度：

* `y < height / 2` → 上半部分 → `prevPage()`
* `y >= height / 2` → 下半部分 → `nextPage()`

Qt 的 `QMouseEvent` 文档中说明，事件携带的是 widget 局部坐标（`position()`/`pos()`），可用于此类区域判断。

### 4.3.1 右键事件拦截（锁定模式）

```cpp
bool MainWindow::eventFilter(QObject *obj, QEvent *e)
{
    if (lockedMode && e->type() == QEvent::MouseButtonPress) {
        auto *m = static_cast<QMouseEvent*>(e);
        if (m->button() == Qt::RightButton) {
            handlePageClick(m->position().toPoint());  // Qt 6 写法
            return true; // 阻止事件穿透
        }
    }
    return QObject::eventFilter(obj, e);
}
```

### 4.3.2 右键区域判断与翻页实现

```cpp
void MainWindow::handlePageClick(const QPoint &localPos)
{
    int h = this->height();
    if (localPos.y() < h / 2) {
        prevPage();   // 上半部分 → 上一页
    } else {
        nextPage();   // 下半部分 → 下一页
    }
}
```

该方案不依赖 WebEngine，仅基于窗口大小和鼠标位置，因此在透明/非透明模式下都有效。

---

## 4.4 锁定 / 未锁定 / Ctrl 临时解锁行为矩阵

### 4.4.1 状态定义

* **锁定模式**（🔒=ON）：透明覆盖层，尽可能不抢焦点、不打断工作。
* **未锁定模式**（🔒=OFF）：正常阅读器，可拖动、滚动、交互。
* **临时解锁**（Ctrl 按住）：在🔒基础上临时表现为未🔒，松开后恢复🔒并还焦。

### 4.4.2 鼠标行为矩阵

| 操作       | 🔒 锁定 (无 Ctrl) | 🔒 锁定 + Ctrl 按住  | 🔓 未锁定           |
| -------- | -------------- | ---------------- | ---------------- |
| 左键       | 穿透到底层应用        | 整个窗口可按住拖动移动      | 整个窗口可按住拖动移动      |
| 中键       | 穿透             | 可自定义（如翻页/无操作）    | 可自定义             |
| 滚轮       | 穿透到底层应用        | 滚动当前 Markdown 页面 | 滚动当前 Markdown 页面 |
| 右键       | 上/下翻页（按上下半区）   | 标准右键或自定义行为       | 标准右键或自定义行为       |
| 拖动窗口     | 禁止             | 允许（整窗拖动）         | 允许（整窗拖动）         |
| 焦点归属     | 原工作应用          | 可能暂时在阅读器         | 阅读器窗口            |
| Ctrl 松开后 | 不适用            | 立即恢复🔒 + 焦点回原应用  | 不适用              |

---

## 4.5 整个窗口可拖动的实现

在 frameless 窗口中，常见做法是在 **顶层窗口（QMainWindow / QWidget）中重写 mousePressEvent / mouseMoveEvent / mouseReleaseEvent**，记录按下时的偏移量，在移动时调用 `move()`。

### 4.5.1 成员变量

```cpp
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    bool   m_dragging = false;
    QPoint m_dragOffset;   // 鼠标相对于窗口左上角的偏移

    bool lockedMode = false;        // 🔒 状态
    bool tempUnlockedByCtrl = false; // Ctrl 临时解锁标记（由第 3 章逻辑维护）
};
```

### 4.5.2 拖动逻辑（整窗区域）

```cpp
bool MainWindow::canDragWindow(QMouseEvent *event) const
{
    // 仅在未🔒或 Ctrl 临时解锁状态下允许整窗拖动
    if (!lockedMode)
        return true;
    if (lockedMode && tempUnlockedByCtrl)
        return true;
    return false;
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && canDragWindow(event)) {
        m_dragging = true;
        // 使用 globalPos 与 frameGeometry 的 topLeft 计算偏移
        m_dragOffset = event->globalPos() - frameGeometry().topLeft();
        event->accept();
        return;
    }
    QMainWindow::mousePressEvent(event);
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        // 整个窗口跟随鼠标移动
        move(event->globalPos() - m_dragOffset);
        event->accept();
        return;
    }
    QMainWindow::mouseMoveEvent(event);
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_dragging) {
        m_dragging = false;
        event->accept();
        return;
    }
    QMainWindow::mouseReleaseEvent(event);
}
```

> 实际注意点：
>
> * 如果中央是 `QWebEngineView`，鼠标事件会先发送给它。为了确保整窗拖动生效，你可以：
>
>   * 给 `QWebEngineView` 安装一个 `eventFilter`，在其中将左键按下/移动事件转发给 `MainWindow` 的拖动逻辑；或者
>   * 在 `QWebEngineView` 上设置一个透明 overlay widget，由 overlay 接管拖动，再把事件决定是否传给 WebEngine。
> * 具体选择取决于你对“拖动 vs 文本选择/点击链接”的取舍，可以按需求微调。

---

## 4.6 滚轮行为细节

* 在 **未🔒 / Ctrl 临时解锁** 时：

  * 中央 `QWebEngineView` 自动处理 `QWheelEvent`，用于滚动内容，无需额外代码。
* 在 **🔒 锁定模式** 时：

  * 通过 `Qt::WindowTransparentForInput` 让窗口整体不再接收鼠标/滚轮事件（按钮栏除外），滚轮事件由光标下的底层应用接收，实现“不打断当前工作应用”的目标。

---

## 4.7 小结

本章最终设计为：

1. 统一窗口尺寸规划与最小尺寸策略；
2. 利用窗口高度将右键翻页分为“上半区上一页 / 下半区下一页”；
3. 定义锁定 / 未锁定 / Ctrl 临时解锁三种状态下的完整鼠标行为矩阵；
4. 使用 **整窗拖动逻辑**（基于 `globalPos` + `frameGeometry()`）在未🔒或 Ctrl 临时解锁时实现任意区域拖动窗口；
5. 在锁定模式下让滚轮事件穿透到底层应用，在未锁定/临时解锁模式下用于滚动当前 Markdown 内容。

整体方案基于 Qt 的事件系统和 frameless window 实战经验，易于实现和维护。

```

```


# 5：Markdown 渲染系统（Qt WebEngine + marked.js）

本章定义 **Markdown → HTML → WebEngineView** 的完整渲染方案，参考 Qt 官方 *WebEngine Markdown Editor* 示例与 Qt 文档，结合 `marked.js` 实现稳定、可扩展的渲染管线。 

---
# 后续补充：

- 所有内部链接统一写成 GitHub 风格：
- [标题](相对路径.md)，路径相对于当前文档或仓库根目录。

- 链接文字必须写在一行内：

- [] 内部不能换行；

- () 内部的 URL 也不能跨行；

- 如需在视觉上换行，使用两个空格 + 回车、反斜杠 \ 或 <br/>。

---

## 5.1 设计目标

1. **渲染稳定、可控**  
   - 采用 `marked.js` 作为 Markdown 解析/渲染引擎（Qt 官方 Markdown 示例也是用它）。

2. **渲染逻辑与 C++ 解耦**  
   - C++：负责读取 `.md` 文本、选择文件、记录阅读位置等。  
   - HTML/JS：负责 Markdown → HTML 渲染和 UI 表现。  
   - 使用 `QWebChannel` 或 `runJavaScript()` 传递 Markdown 文本。

3. **方便做主题 / 透明效果**  
   - 所有样式通过 CSS 控制：字体、颜色、透明度、背景颜色/图片等。  

4. **性能可接受**  
   - 打开程序时仅加载一次 `index.html`。  
   - 切换 Markdown 文档时只更新文本，不重新加载 HTML 页面。

---

## 5.2 渲染架构总览

数据流：

```text
.md 文件 → C++（读取 UTF-8 文本）
        → QWebChannel / runJavaScript 把纯文本发给 JS
        → JS: marked.parse(markdownText)
        → 生成 HTML 字符串
        → innerHTML 注入到预览 DIV
        → CSS 控制显示效果（字体/颜色/透明度/背景）
        → QWebEngineView 展示
````

架构与 Qt 官方 WebEngine Markdown Editor / Recipe Browser 示例一致：使用 `QWebEngineView`，加载一个 `index.html`，在其中引用 `marked.js` 和 `qwebchannel.js`，通过 WebChannel 传递 markdown 内容。([麻省理工学院资料库][1])

---

## 5.3 前端资源与 HTML 模板

### 5.3.1 资源组织（qrc）

将前端文件统一打包进 Qt 资源：

```text
:/resources/index.html
:/resources/markdown.css
:/resources/reader_theme.css
:/resources/3rdparty/marked.min.js
```

Qt 官方示例也是通过 `qrc:/index.html` 加载 HTML，并从资源中引用 `marked.min.js`、`markdown.css` 等。([麻省理工学院资料库][1])

### 5.3.2 index.html 示例

```html
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <title>Transparent MD Reader</title>

  <!-- Markdown 基础样式 -->
  <link rel="stylesheet" href="qrc:/resources/markdown.css">
  <!-- 自定义主题：字体、颜色、透明度、背景等 -->
  <link rel="stylesheet" href="qrc:/resources/reader_theme.css">

  <!-- Markdown 引擎：marked.js（Qt 官方 Markdown 示例同款） -->
  <script src="qrc:/resources/3rdparty/marked.min.js"></script>
  <!-- Qt WebChannel -->
  <script src="qrc:///qtwebchannel/qwebchannel.js"></script>
</head>
<body>
  <!-- 渲染后的 HTML 显示容器 -->
  <div id="preview" class="md-preview"></div>

  <script>
    // marked 全局配置（可按需要调整）
    marked.setOptions({
      gfm: true,
      breaks: true,
      headerIds: true,
      mangle: false
    });

    function renderMarkdown(markdownText) {
      const html = marked.parse(markdownText || "");
      document.getElementById("preview").innerHTML = html;
    }

    // 通过 QWebChannel 接收 C++ 传来的 markdown 文本
    new QWebChannel(qt.webChannelTransport, function(channel) {
      window.backend = channel.objects.backend;

      // 首次内容
      renderMarkdown(backend.markdownText);

      // 后续切换文件
      backend.markdownChanged.connect(function(newText) {
        renderMarkdown(newText);
      });
    });
  </script>
</body>
</html>
```

说明：

* `qrc:/` 路径加载 HTML/CSS/JS 是 Qt 官方示例推荐方式。([麻省理工学院资料库][1])
* `marked.parse(markdownText)` 是官方文档推荐 API。([Marked][2])

---

## 5.4 C++ 端初始化 WebEngine 与 WebChannel

### 5.4.1 创建 WebEngineView 并加载 index.html

```cpp
// MainWindow.h
#include <QWebEngineView>
#include <QWebChannel>

class Backend;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    QWebEngineView *m_view = nullptr;
    Backend        *m_backend = nullptr;
};
```

```cpp
// MainWindow.cpp
#include "MainWindow.h"
#include "Backend.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_view = new QWebEngineView(this);
    setCentralWidget(m_view);

    // 创建 WebChannel 后端对象
    m_backend = new Backend(this);

    auto *channel = new QWebChannel(this);
    channel->registerObject(QStringLiteral("backend"), m_backend);
    m_view->page()->setWebChannel(channel);  // 与 index.html 中的 qwebchannel.js 对应

    // 从资源加载 HTML 页面
    m_view->setUrl(QUrl(QStringLiteral("qrc:/resources/index.html")));
}
```

* `QWebEngineView::setUrl()` 加载 `qrc:/` 资源是官方文档支持的用法。([QTHub][3])
* `setWebChannel()` + `qwebchannel.js` 是 Qt 官方推荐的 C++ ↔ JS 通信方式。([Stack Overflow][4])

### 5.4.2 Backend 类（传递 markdown 文本）

```cpp
// Backend.h
#include <QObject>

class Backend : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString markdownText READ markdownText WRITE setMarkdownText NOTIFY markdownChanged)
public:
    explicit Backend(QObject *parent = nullptr) : QObject(parent) {}

    QString markdownText() const { return m_text; }

public slots:
    void setMarkdownText(const QString &text) {
        if (text == m_text)
            return;
        m_text = text;
        emit markdownChanged(m_text);
    }

signals:
    void markdownChanged(const QString &);

private:
    QString m_text;
};
```

---

## 5.5 加载 .md 文件并触发渲染

当用户打开某个 `.md` 文件时：

```cpp
#include <QFile>
#include <QTextStream>

QString loadMarkdownFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();

    QTextStream in(&f);
    in.setCodec("UTF-8");
    return in.readAll();
}

void MainWindow::openMarkdownFile(const QString &path)
{
    const QString text = loadMarkdownFile(path);
    currentFilePath = path;          // 用于阅读位置、历史记录等
    m_backend->setMarkdownText(text);
    // 第 9 章中会在渲染后调用 JS 恢复滚动位置
}
```

* 建议统一使用 UTF-8 编码，避免中英混合出现乱码问题。
* WebEngine 默认按 UTF-8 解码 HTML 和 JS，和这里保持一致。

---

## 5.6 链接渲染与点击行为

### 5.6.1 渲染层：marked.js + WebEngine

Markdown 中的链接：

```markdown
[官方文档](https://doc.qt.io/)
[下一节](102-basics_zh.md)
```

`marked.parse()` 会生成：

```html
<p><a href="https://doc.qt.io/">官方文档</a></p>
<p><a href="102-basics_zh.md">下一节</a></p>
```

`QWebEngineView` 会像浏览器一样渲染 `<a>` 标签——这是 Chromium 的默认行为。([Marked][2])

### 5.6.2 链接样式与 hover 高亮

在 `reader_theme.css` 中增加：

```css
.md-preview a {
    color: rgba(80, 160, 255, 0.95);
    text-decoration: underline;
    cursor: pointer;
    transition: opacity 0.12s ease, color 0.12s ease;
}

.md-preview a:hover {
    opacity: 0.8;
    /* 或者：color: rgba(120, 200, 255, 1.0); */
}
```

效果：

* 鼠标移动到链接上：

  * 光标变成手型（`cursor: pointer` + 浏览器默认行为）
  * 透明度或颜色略变，明确显示“可点击”提示。

这些都是纯前端行为，与 C++ 不冲突。

### 5.6.3 点击行为：拦截导航请求

我们自定义 `QWebEnginePage`，控制点击后的行为：

* **内部 Markdown 链接**：交给阅读器打开对应 `.md` 文件。
* **外部 http/https 链接**：交给系统默认浏览器。

```cpp
// MarkdownPage.h
#include <QWebEnginePage>

class MarkdownPage : public QWebEnginePage
{
    Q_OBJECT
public:
    using QWebEnginePage::QWebEnginePage;

signals:
    void openMarkdown(const QUrl &url);

protected:
    bool acceptNavigationRequest(const QUrl &url,
                                 NavigationType type,
                                 bool isMainFrame) override;
};
```

```cpp
// MarkdownPage.cpp
#include "MarkdownPage.h"
#include <QDesktopServices>

static bool isInternalMarkdown(const QUrl &url)
{
    // 简单判断：以 .md 结尾的相对路径视为内部文档
    const QString path = url.path();
    return path.endsWith(".md", Qt::CaseInsensitive);
}

bool MarkdownPage::acceptNavigationRequest(const QUrl &url,
                                           NavigationType type,
                                           bool isMainFrame)
{
    if (type == QWebEnginePage::NavigationTypeLinkClicked) {
        if (isInternalMarkdown(url)) {
            emit openMarkdown(url);           // 交给 MainWindow 处理
        } else {
            QDesktopServices::openUrl(url);   // 外部链接交给系统浏览器
        }
        return false; // 阻止 WebEngine 默认加载
    }
    // 其它导航（首次加载 index.html 等）走默认逻辑
    return QWebEnginePage::acceptNavigationRequest(url, type, isMainFrame);
}
```

Qt 文档指出，重载 `acceptNavigationRequest()` 可以让应用接管导航请求（包括点击链接），`NavigationTypeLinkClicked` 表示用户触发的链接点击事件。([麻省理工学院资料库][5])

在创建 `QWebEngineView` 时使用自定义 Page：

```cpp
auto *page = new MarkdownPage(m_view);
m_view->setPage(page);
connect(page, &MarkdownPage::openMarkdown,
        this, &MainWindow::handleOpenMarkdownUrl);
```

在 `handleOpenMarkdownUrl()` 中，把 `url.path()` 解析成本地 md 文件路径，然后调用 `openMarkdownFile(localPath)` 即可。

---

## 5.7 与锁定 / Ctrl 模式的配合

结合第 3、4 章的逻辑：

* **🔒 锁定 + 未按 Ctrl**

  * 整窗启用 `Qt::WindowTransparentForInput`，除按钮栏外不接收鼠标事件。
  * 鼠标移动到链接上不会触发 hover / click（事件已经穿透到底层应用）。
  * 这是刻意设计：锁定模式下只当“透明阅读背景”，不干扰工作流。

* **🔓 未锁定 或 Ctrl 按住临时解锁**

  * 关闭 `WindowTransparentForInput`，窗口正常接收鼠标事件。
  * 鼠标移动到链接上：hover 高亮、光标变手型。
  * 左键点击链接：触发 `NavigationTypeLinkClicked` → 进入 `acceptNavigationRequest()`，再按内链/外链处理。

由于：

* “是否接收鼠标事件”由 Qt 窗口 flag 控制；
* “链接点击行为”由 `QWebEnginePage::acceptNavigationRequest()` 控制；

两者在不同层面工作，不会出现逻辑冲突：

* 锁定时：链接无交互，是背景。
* 可交互时：链接行为完全正常，并且在我们的控制之下（内部跳转 or 系统浏览器）。

---

## 5.8 性能与刷新策略

1. **只加载一次 index.html**

   * 程序启动时 `setUrl("qrc:/resources/index.html")`。
   * 后续文件切换仅更新 `Backend::markdownText`。([麻省理工学院资料库][1])

2. **避免频繁销毁 WebEngineView / Page**

   * `QWebEngineView` 和 `MarkdownPage` 在 MainWindow 生命周期内保持不变。
   * 只在需要时更新文本和滚动位置。

3. **滚动位置恢复放在渲染之后**

   * 第 9 章会专门讲：

     * 通过 JS 获取/设置 `window.pageYOffset`；
     * 每次渲染完成后（例如 Markdown 内容更新后），再调用 `window.scrollTo(0, savedY)` 恢复阅读位置。

---

## 5.9 小结

本章确定了你的项目在 Markdown 渲染上的完整方案：

* 使用 Qt 官方 WebEngine Markdown Editor 示例同款思路：
  `QWebEngineView + index.html + marked.js + qwebchannel.js`。([麻省理工学院资料库][1])
* 所有前端资源放在 qrc 中，通过 `qrc:/` 路径加载。([麻省理工学院资料库][1])
* C++ 只负责管理 Markdown 文本和业务逻辑，不直接参与 Markdown 解析。
* JS 使用 `marked.parse()` 进行渲染，CSS 负责所有展示效果（颜色/透明度/背景）。([Marked][2])
* 自定义 `QWebEnginePage::acceptNavigationRequest()` 控制链接点击行为，实现内部文档跳转与外部浏览器打开的区分。([麻省理工学院资料库][5])
* 与锁定 / Ctrl 模式完美配合：锁定时全部穿透，临时解锁/未锁定时链接正常 hover + clickable。

在此基础上，第 6 章将专门设计 **主题系统**：统一配置文字颜色、透明度、背景颜色/图片、全局透明风格等。

```
```

[1]: https://stuff.mit.edu/afs/athena.mit.edu/software/texmaker_v5.0.2/qt57/doc/qtwebengine/qtwebengine-webenginewidgets-markdowneditor-example.html?utm_source=chatgpt.com "WebEngine Markdown Editor Example"
[2]: https://marked.js.org/?utm_source=chatgpt.com "Marked Documentation"
[3]: https://qthub.com/static/doc/qt5/qtwebengine/qwebengineview.html?utm_source=chatgpt.com "QWebEngineView Class | Qt WebEngine 5.15.1"
[4]: https://stackoverflow.com/questions/28565254/how-to-use-qt-webengine-and-qwebchannel?utm_source=chatgpt.com "How to use Qt WebEngine and QWebChannel?"
[5]: https://stuff.mit.edu/afs/athena/software/texmaker_v5.0.2/qt57/doc/qtwebengine/qwebenginepage.html?utm_source=chatgpt.com "QWebEnginePage Class | Qt WebEngine 5.7"


# 6：主题系统设计（文字 / 背景 / 透明度）  
> ⚠ 本章按照「阅读窗口只负责阅读，所有设置都放到系统托盘」的原则重新设计。

---

## 6.1 设计原则

1. **阅读窗口只做一件事：展示内容 + 🔒 + X**  
   - 阅读窗口不出现任何设置按钮（不弹设置面板、不出现主题下拉框等）。  
   - 阅读窗口只保留：  
     - `X`：关闭  
     - `🔒`：锁定/解锁（作用见第 3、4 章）。

2. **所有设置都集中在系统托盘菜单**  
   - 主题切换（深色/浅色/纸张/壁纸）。  
   - 文字/背景透明度调整（例如：低/中/高透明预设）。  
   - 其他后续设置（是否记忆窗口位置、是否开机自启等）也都放托盘。  

3. **托盘菜单 → 发信号 → MainWindow / Backend → JS/CSS 更新主题**  
   - 托盘只是一个“控制面板”，不直接改 UI。  
   - 真正的视觉变化由 WebEngine + CSS 完成。

---

## 6.2 样式结构回顾（保持不变）

前端仍采用：

- 容器：`<div id="preview" class="md-preview theme-dark"></div>`  
- `markdown.css`：Markdown 基础语义样式（标题、段落、列表、代码等）。  
- `reader_theme.css`：主题 / 颜色 / 背景 / 透明度控制。  

核心思路仍是：

- 用 `theme-dark / theme-light / theme-paper / theme-wallpaper-dark` 这类 class 来区分主题；
- 用 `rgba(...)` 控制文字颜色和背景透明度；
- 如果需要调整整体透明度，额外在 JS 中注入 `style` 控制 `.md-preview { opacity: X; }`。

这一层不因为“设置入口改到托盘”而改变。

---

## 6.3 主题 CSS 示例（保持核心不变）

### 6.3.1 深色透明主题

```css
.md-preview.theme-dark {
    color: rgba(240, 240, 245, 0.94);
    background-color: rgba(15, 15, 20, 0.25);
}

.md-preview.theme-dark h1,
.md-preview.theme-dark h2 {
    color: rgba(255, 255, 255, 0.98);
}

.md-preview.theme-dark code {
    background-color: rgba(255, 255, 255, 0.06);
}

.md-preview.theme-dark pre code {
    background-color: rgba(0, 0, 0, 0.45);
}
````

### 6.3.2 浅色半透明主题

```css
.md-preview.theme-light {
    color: rgba(30, 30, 35, 0.96);
    background-color: rgba(245, 245, 255, 0.35);
}

.md-preview.theme-light h1,
.md-preview.theme-light h2 {
    color: rgba(10, 10, 15, 0.98);
}
```

### 6.3.3 纸张 / 壁纸主题

```css
.md-preview.theme-paper {
    color: rgba(45, 45, 40, 0.96);
    background-color: rgba(255, 255, 255, 0.85);
    background-image: url("qrc:/resources/backgrounds/paper_texture.png");
    background-size: cover;
    background-repeat: no-repeat;
    background-position: center top;
}
```

```css
.md-preview.theme-wallpaper-dark {
    color: rgba(240, 240, 245, 0.94);
    background-color: rgba(0, 0, 0, 0.35);
    background-image: url("qrc:/resources/backgrounds/wallpaper_dark.jpg");
    background-size: cover;
    background-position: center center;
}
```

---

## 6.4 链接视觉（与第 5 章一致）

```css
.md-preview a {
    color: rgba(80, 160, 255, 0.95);
    text-decoration: underline;
    cursor: pointer;
    transition: opacity 0.12s ease, color 0.12s ease;
}

.md-preview a:hover {
    opacity: 0.8;
}
```

必要时可按主题调整：

```css
.md-preview.theme-light a {
    color: rgba(40, 100, 200, 0.95);
}
```

---

## 6.5 JS 端的主题切换 API（不变）

仍然在 `index.html` 中提供可供 C++ 调用的函数：

```js
function setTheme(themeName) {
  const el = document.getElementById("preview");
  el.classList.remove("theme-dark", "theme-light", "theme-paper", "theme-wallpaper-dark");
  el.classList.add(themeName);
}
```

可选：一个整体透明度接口，用于日后“透明度预设”：

```js
function setContentOpacity(opacity) {
  // 0.1 ~ 1.0 之间
  opacity = Math.max(0.1, Math.min(1.0, opacity));
  document.getElementById("preview").style.opacity = opacity;
}
```

---

## 6.6 C++ 端：Backend 信号设计（配合托盘）

为了让 **系统托盘** 控制主题和透明度，我们在 C++ 侧做两个层次：

1. 托盘菜单 → 发槽函数到 MainWindow（比如 `onThemeDark()`, `onOpacityLow()`）。
2. MainWindow → 调用 `Backend` / 直接 `runJavaScript`，通知 WebEngine 更新主题。

### 6.6.1 Backend 信号扩展

```cpp
class Backend : public QObject
{
    Q_OBJECT
    // ...

signals:
    void markdownChanged(const QString &text);
    void themeChanged(const QString &themeName);
    void contentOpacityChanged(double opacity);
};
```

在 `index.html` / JS 里监听：

```js
new QWebChannel(qt.webChannelTransport, function(channel) {
  window.backend = channel.objects.backend;

  renderMarkdown(backend.markdownText);
  backend.markdownChanged.connect(renderMarkdown);

  if (backend.themeChanged) {
    backend.themeChanged.connect(function(themeName) {
      setTheme(themeName);
    });
  }

  if (backend.contentOpacityChanged) {
    backend.contentOpacityChanged.connect(function(opacity) {
      setContentOpacity(opacity);
    });
  }
});
```

---

## 6.7 托盘菜单触发主题 / 透明度（与第 10 章联动）

系统托盘部分会在第 10 章详细写，这里只关心它如何“调用主题系统”。

### 6.7.1 托盘菜单结构示例（伪代码）

在 `MainWindow` / `TrayManager` 中：

```cpp
void MainWindow::createTrayMenu()
{
    QMenu *menu = new QMenu(this);

    // 主题子菜单
    QMenu *themeMenu = menu->addMenu(tr("主题 (&T)"));
    QAction *actThemeDark  = themeMenu->addAction(tr("深色透明"));
    QAction *actThemeLight = themeMenu->addAction(tr("浅色透明"));
    QAction *actThemePaper = themeMenu->addAction(tr("纸张风格"));

    connect(actThemeDark,  &QAction::triggered, this, [this]{ applyTheme(QStringLiteral("theme-dark")); });
    connect(actThemeLight, &QAction::triggered, this, [this]{ applyTheme(QStringLiteral("theme-light")); });
    connect(actThemePaper, &QAction::triggered, this, [this]{ applyTheme(QStringLiteral("theme-paper")); });

    // 透明度子菜单（简单预设：低 / 中 / 高）
    QMenu *opacityMenu = menu->addMenu(tr("内容透明度 (&O)"));
    QAction *actOpacityLow    = opacityMenu->addAction(tr("低（更不透明）"));
    QAction *actOpacityMedium = opacityMenu->addAction(tr("中"));
    QAction *actOpacityHigh   = opacityMenu->addAction(tr("高（更透明）"));

    connect(actOpacityLow,    &QAction::triggered, this, [this]{ applyContentOpacity(0.95); });
    connect(actOpacityMedium, &QAction::triggered, this, [this]{ applyContentOpacity(0.85); });
    connect(actOpacityHigh,   &QAction::triggered, this, [this]{ applyContentOpacity(0.7); });

    // 其它托盘项：打开文件、退出等（第 10 章展开）
    // ...

    m_trayIcon->setContextMenu(menu);
}
```

> 注意：
>
> * 这一切发生在“系统托盘菜单”里，而不是阅读窗口 UI。
> * 阅读窗口只显示内容＋🔒＋X，符合你的要求。

### 6.7.2 托盘调用主题接口

MainWindow 中实现 `applyTheme()` 和 `applyContentOpacity()`：

```cpp
void MainWindow::applyTheme(const QString &themeName)
{
    if (!m_backend)
        return;
    emit m_backend->themeChanged(themeName);
    // 将 themeName 写入配置文件，下一次启动自动恢复（可在第 14 章实现）
}

void MainWindow::applyContentOpacity(double opacity)
{
    if (!m_backend)
        return;
    emit m_backend->contentOpacityChanged(opacity);
    // 同样可以保存到配置
}
```

---

## 6.8 阅读窗口不再出现任何设置 UI

结合上面设计，我们明确约束：

* **阅读窗口 UI 元素：**

  * 标题栏区域（可选、可以隐藏）；
  * `X` 按钮：关闭；
  * `🔒` 按钮：切换锁定状态；
  * 内容区域：只显示 Markdown 渲染内容；

* **不出现在阅读窗口内的东西：**

  * 主题选择按钮 / 下拉框；
  * 透明度滑块；
  * 字体设置按钮；
  * 任何其它“设置类”控件。

所有设置类操作全部由：

> **系统托盘 → 菜单 → 调用 C++ → 通知 WebEngine**

完成。

这样：

* 阅读窗口始终保持“极简、干净、只用来读”；
* 系统托盘承担所有配置/开关角色，非常符合“常驻工具类程序”的设计习惯。

---

## 6.9 小结

1. 主题系统仍采用 `.md-preview + theme-xxx` 的 CSS 结构和 `marked.js` 渲染。
2. 所有主题切换和透明度调整从 **阅读窗口 UI 完全剥离**，统一收口到 **系统托盘菜单**。
3. 托盘菜单通过 `applyTheme()` / `applyContentOpacity()` 调用 `Backend` 信号，最终由 JS `setTheme()` / `setContentOpacity()` 更新视觉。
4. 阅读窗口仅保留 `🔒` 和 `X` 两个按钮，加上 Markdown 渲染内容，不再承担任何设置功能。

后续在第 10 章会完整写出 `QSystemTrayIcon` 的设计与菜单结构，并与本章主题接口打通。

```
```




# 7：文档状态持久化（历史记录 + 阅读进度）

本章设计 **“看过哪些文档 + 每个文档看到哪儿了”** 的完整方案，包括：

- 每个文档的阅读进度（滚动位置）
- 历史记录 / 最近打开列表
- 配置文件的持久化位置（exe 同目录的“便携版” + AppData 安装版）
- 与内部链接跳转（本章 7.8 小节）、拖拽打开（第 9 章）、单实例逻辑（第 8 章）的配合

---

## 7.1 设计目标

1. **打开任意文档时，自动恢复到上次阅读位置**  
2. **记录最近打开的文档列表**（可在托盘菜单里挂一个“最近打开”）  
3. **与内部链接跳转配合**：从 A 文档点链接跳到 B 时，正确保存 A 的进度并记入历史  
4. **配置可靠持久化**：  
   - 开发/便携版：配置文件放在 exe 同目录  
   - 安装版：按照 Windows/Qt 官方惯例放到 AppData 之类目录:contentReference[oaicite:0]{index=0}  

---

## 7.2 数据结构设计

### 7.2.1 单个文档的状态

```cpp
struct DocumentState {
    QString filePath;     // 绝对路径
    qint64  lastScrollY;  // 垂直滚动位置（像素）
    QDateTime lastOpen;   // 最后打开时间
};
````

### 7.2.2 全局状态（用于序列化到 JSON）

```cpp
struct AppState {
    QMap<QString, DocumentState> perFile;   // key = 规范化绝对路径
    QStringList recentFiles;               // 最近打开列表（按时间排序，去重）
};
```

持久化时可以写成一个 JSON：

```json
{
  "version": 1,
  "recentFiles": [
    "D:/docs/learncpp_md/101-intro_zh.md",
    "D:/docs/learncpp_md/102-basics_zh.md"
  ],
  "perFile": {
    "D:/docs/learncpp_md/101-intro_zh.md": {
      "lastScrollY": 1240,
      "lastOpen": "2025-11-16T10:21:33"
    },
    "D:/docs/learncpp_md/102-basics_zh.md": {
      "lastScrollY": 420,
      "lastOpen": "2025-11-16T10:30:12"
    }
  }
}
```

---

## 7.3 配置文件存储位置策略（exe 同目录 + 安装版）

你提到的关键点：

> “持久化的配置文件，生成在 exe 的目录（能不能做成安装包形式，这样配置文件什么的都好设置？）”

这可以这样设计：

### 7.3.1 便携版（绿色版）：配置放 exe 同目录

适合你现在用的场景：一个 `transparent_md_reader.exe` 和一堆配置都在同一目录，拷走整个文件夹就带走所有数据。

路径获取：

```cpp
#include <QCoreApplication>
#include <QDir>

QString configPathPortable()
{
    const QString exeDir = QCoreApplication::applicationDirPath();
    return QDir(exeDir).filePath("transparent_md_reader_state.json");
}
```

**注意：**

* **不要把便携版安装到 `C:\Program Files` 这种目录**，普通用户通常没有写权限，写 exe 目录会失败；这点在 Windows / .NET / Qt 圈子里都被认为是不推荐的。([Super User][1])
* 便携版的典型位置：你的个人工作目录、某个 D 盘工具文件夹、U 盘等。

### 7.3.2 安装版：遵循官方推荐，放 AppData

如果你以后用安装包（Inno Setup / NSIS / Qt Installer Framework 等）安装到 `C:\Program Files\TransparentMdReader`：

* 可执行文件放在 Program Files（只读）
* 配置文件和历史记录应放在 AppData（按用户隔离）([Qt 文档][2])

Qt 官方推荐用 `QStandardPaths` 查“可写配置路径”：([Qt 文档][2])

```cpp
#include <QStandardPaths>

QString configPathAppData()
{
    // AppConfigLocation: 配置文件
    // AppDataLocation:   应用数据（缓存、索引…）
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir dir(base);
    dir.mkpath("."); // 确保目录存在
    return dir.filePath("transparent_md_reader_state.json");
}
```

Windows 上它一般会落在类似：

* `C:\Users\<User>\AppData\Roaming\<OrgName>\<AppName>\` 这样的目录([CopperSpice][3])

### 7.3.3 二合一策略：自动选择 / 开关“便携模式”

你可以约定一个简单规则：

* 如果 exe 目录下存在一个空文件：`portable.flag` → 认为是“便携模式”，配置写 exe 同目录；
* 否则 → 使用 `QStandardPaths::AppConfigLocation`。

伪代码：

```cpp
QString detectConfigPath()
{
    const QString exeDir = QCoreApplication::applicationDirPath();
    const QString portableFlag = QDir(exeDir).filePath("portable.flag");

    if (QFile::exists(portableFlag)) {
        return QDir(exeDir).filePath("transparent_md_reader_state.json");
    }

    // 默认走 AppData（安装版）
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir dir(base);
    dir.mkpath(".");
    return dir.filePath("transparent_md_reader_state.json");
}
```

这样：

* 开发阶段你直接丢个 `portable.flag`，所有配置就在 exe 目录，拷整个文件夹即可。
* 做安装包时，不放这个文件，自动使用标准 AppData 目录，这是官方推荐做法。([Stack Overflow][4])

---

## 7.4 滚动位置采集（当前阅读进度）

### 7.4.1 JS 端：提供获取 scrollY 的 API

在 `index.html` 的 JS 里加：

```js
function getScrollY() {
  return window.pageYOffset || document.documentElement.scrollTop || document.body.scrollTop || 0;
}
```

### 7.4.2 C++ 端：通过 runJavaScript 询问当前滚动位置

在需要保存进度时（比如定时、切换文档、关闭程序前）调用：

```cpp
void MainWindow::captureScrollPosition()
{
    m_view->page()->runJavaScript(
        "getScrollY();",
        [this](const QVariant &result) {
            bool ok = false;
            const qreal y = result.toReal(&ok);
            if (!ok) return;

            const qint64 scrollY = static_cast<qint64>(y);

            DocumentState state;
            state.filePath = normalizedPath(currentFilePath);
            state.lastScrollY = scrollY;
            state.lastOpen = QDateTime::currentDateTime();

            m_state.perFile[state.filePath] = state;
            updateRecentList(state.filePath);
            m_dirty = true; // 标记需要写回配置
        }
    );
}
```

> 建议：
>
> * 不要在每一小步滚动时都写文件，可以仅在：
>
>   * 切换文档前
>   * 程序退出前
>   * 定时（比如 30 秒写一次）
> * `m_dirty` 标记配合定时器统一 flush。

---

## 7.5 打开文档时恢复阅读位置

当你调用 `openMarkdownFile(path)` 时：

1. 按第 5 章流程加载 `.md` 内容并渲染。
2. 从 `AppState::perFile` 中查找该路径的 `lastScrollY`。
3. 文档渲染完成后，通过 JS 滚动到该位置。

### 7.5.1 恢复逻辑

```cpp
void MainWindow::openMarkdownFile(const QString &path)
{
    const QString normPath = normalizedPath(path);
    currentFilePath = normPath;

    // 读取文件内容、通过 backend->setMarkdownText(...) 渲染
    const QString text = loadMarkdownFile(normPath);
    backend->setMarkdownText(text);

    // 查找是否有保存过的滚动位置
    qint64 scrollY = 0;
    if (m_state.perFile.contains(normPath)) {
        scrollY = m_state.perFile.value(normPath).lastScrollY;
    }

    // 在页面渲染完成后再滚动
    restoreScrollAfterRender(scrollY);
}
```

`restoreScrollAfterRender` 可以简单通过 `runJavaScript` 延迟执行：

```cpp
void MainWindow::restoreScrollAfterRender(qint64 scrollY)
{
    // 简化版：直接调用 scrollTo
    const QString js = QStringLiteral(
        "window.scrollTo(0, %1);"
    ).arg(scrollY);

    // 可以在一定延迟后调用，或在 markdownChanged → 渲染完后触发
    QTimer::singleShot(100, this, [this, js]{
        m_view->page()->runJavaScript(js);
    });
}
```

后面如果需要“锚点优先 / 滚动位置优先”的更复杂逻辑，可以叠加规则：如果这次是从 `xxx.md#anchor` 跳过来的，则先 anchor，再叠加滚动，或反之。

---

## 7.6 历史记录与“最近打开”

### 7.6.1 最近打开列表（Recent Files）

设计一个简单更新函数：

```cpp
void MainWindow::updateRecentList(const QString &filePath)
{
    const QString norm = normalizedPath(filePath);
    m_state.recentFiles.removeAll(norm);
    m_state.recentFiles.prepend(norm);

    const int maxRecent = 20;
    if (m_state.recentFiles.size() > maxRecent) {
        m_state.recentFiles = m_state.recentFiles.mid(0, maxRecent);
    }

    m_dirty = true;
}
```

托盘菜单可以按 `m_state.recentFiles` 动态生成（类似第 6 章主题菜单），点击后调用 `openMarkdownFile()`。

### 7.6.2“返回上一文档”（简单版历史栈）

可以维护两个栈：

```cpp
QStack<QString> m_backStack;
QStack<QString> m_forwardStack;
```

当你从 A 跳到 B（包括拖拽打开 / 链接跳转）时：

```cpp
void MainWindow::navigateTo(const QString &targetPath)
{
    // 保存当前 A 的滚动位置
    captureScrollPosition();

    if (!currentFilePath.isEmpty()) {
        m_backStack.push(currentFilePath);
        m_forwardStack.clear(); // 新导航清空 forward
    }

    openMarkdownFile(targetPath);
}
```

在托盘菜单里可以有“后退 / 前进”：

```cpp
void MainWindow::goBack()
{
    if (m_backStack.isEmpty())
        return;

    captureScrollPosition();  // 保存当前

    const QString prev = m_backStack.pop();
    m_forwardStack.push(currentFilePath);
    openMarkdownFile(prev);
}

void MainWindow::goForward()
{
    if (m_forwardStack.isEmpty())
        return;

    captureScrollPosition();

    const QString next = m_forwardStack.pop();
    m_backStack.push(currentFilePath);
    openMarkdownFile(next);
}
```

这些栈可以选择是否持久化到配置文件；最基本需求可以只在当前进程内生效，不写入 JSON。

---

## 7.7 配置文件读写流程

### 7.7.1 启动时读取

```cpp
void MainWindow::loadAppState()
{
    const QString path = detectConfigPath();
    QFile f(path);
    if (!f.exists())
        return;

    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    const QByteArray data = f.readAll();
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject())
        return;

    const QJsonObject root = doc.object();
    const QJsonObject perFile = root.value("perFile").toObject();
    for (auto it = perFile.begin(); it != perFile.end(); ++it) {
        const QString filePath = it.key();
        const QJsonObject obj = it.value().toObject();
        DocumentState st;
        st.filePath = filePath;
        st.lastScrollY = obj.value("lastScrollY").toInteger(0);
        st.lastOpen = QDateTime::fromString(obj.value("lastOpen").toString(), Qt::ISODate);
        m_state.perFile.insert(filePath, st);
    }

    const QJsonArray recent = root.value("recentFiles").toArray();
    for (const auto &v : recent) {
        m_state.recentFiles.append(v.toString());
    }
}
```

### 7.7.2 退出或定时写回

```cpp
void MainWindow::saveAppState()
{
    if (!m_dirty)
        return;

    QJsonObject root;
    QJsonObject perFileObj;

    for (auto it = m_state.perFile.constBegin(); it != m_state.perFile.constEnd(); ++it) {
        const DocumentState &st = it.value();
        QJsonObject obj;
        obj.insert("lastScrollY", static_cast<qint64>(st.lastScrollY));
        obj.insert("lastOpen", st.lastOpen.toString(Qt::ISODate));
        perFileObj.insert(it.key(), obj);
    }

    root.insert("version", 1);
    root.insert("perFile", perFileObj);

    QJsonArray recentArr;
    for (const auto &p : m_state.recentFiles)
        recentArr.append(p);
    root.insert("recentFiles", recentArr);

    const QJsonDocument doc(root);
    const QString path = detectConfigPath();

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return;

    f.write(doc.toJson(QJsonDocument::Indented));
    f.close();
    m_dirty = false;
}
```

时机：

* `MainWindow` 析构 / `closeEvent()` 中调用一次。
* 也可以用 `QTimer` 每隔 30 秒检查 `m_dirty`，为安全再写一次。

---

## 7.8 与其他模块的配合

1. **第 3–4 章（窗口锁定 / Ctrl 临时解锁）**

   * 状态持久化与锁定行为是解耦的，仅在“文档切换”/“关闭程序”时读取滚动位置；
   * 是否锁定不影响位置存储逻辑。

2. **第 5 章（Markdown 渲染）**

   * 打开文档 → 渲染 → 恢复 scrollY；
   * 所有滚动 API 都通过 `runJavaScript` 调用 WebEngine。

3. **第 7 章（链接跳转）**

   * 在 `onOpenMarkdownLink()` 中调用 `navigateTo()` / `captureScrollPosition()`，保证从 A 跳 B 时记录 A 的进度；
   * 便于“返回上一文档”时恢复正确位置。

4. **拖拽打开 / 单实例**

   * 拖 md 到 exe / 窗口 → 当成普通“导航到新文档”，也复用 `navigateTo()`；
   * 单实例模式下，如果第二次拖拽新文件到 exe，可以通过 IPC 或命令行参数通知已运行实例，同样走 `navigateTo()`。

---

## 7.9 小结

1. 将 **历史记录 + 阅读位置** 合并为统一的“文档状态持久化”模块：

   * 每个文档记录 `lastScrollY` + `lastOpen`;
   * 全局保存 `recentFiles` 列表；
   * 可选 back/forward 栈实现“后退/前进”。

2. 配置文件路径设计为“两用模式”：

   * 开发/便携版：exe 目录（确保放在可写目录）；
   * 安装版：使用 Qt 官方推荐的 `QStandardPaths::AppConfigLocation` / AppData 目录。([Qt 文档][2])

3. 滚动位置通过 JS `getScrollY()` 和 `window.scrollTo()` 与 C++ 交互，写入/读取 JSON 配置；写回时机为切换文档 / 退出 / 定时 flush。

4. 这一章打好了“记忆你阅读进度”的基础，后续如果想加：

   * “继续阅读上一篇”
   * “显示最近 N 篇文档”
   * “按最后打开时间排序的历史列表”
     都可以直接在这套结构上扩展。

```
```

[1]: https://superuser.com/questions/1445143/saving-data-program-files-vs-appdata-windows?utm_source=chatgpt.com "Saving data: Program Files vs AppData [Windows]"
[2]: https://doc.qt.io/qt-6/qstandardpaths.html?utm_source=chatgpt.com "QStandardPaths Class | Qt Core | Qt 6.10.0"
[3]: https://www.copperspice.com/docs/cs_api/class_qsettings.html?utm_source=chatgpt.com "QSettings Class Reference"
[4]: https://stackoverflow.com/questions/12427245/installing-in-program-files-vs-appdata?utm_source=chatgpt.com "Installing in Program Files vs. Appdata"

### 7.10 程序启动 & 拖动到 exe 的行为（细节补充）

你提到的两个细节，其实就是这两种场景：

1. **双击 exe 启动（无参数）**：自动打开“上次看的那篇文档”，并恢复阅读位置。  
2. **已经打开一个文档，但又把其它 `.md` 拖到 exe 上**：  
   - 先保存当前文档的阅读位置等状态；  
   - 再根据被拖动的 `.md` 路径，打开对应文档，并恢复它的阅读位置。

这两个需求，前面第 8 章的设计可以直接支撑，我们补一小节把流程写清楚。

---

#### 7.10.1 双击 exe：自动打开上次阅读的文档

启动流程可以这样设计：

```cpp
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    MainWindow w;

    w.loadAppState(); // 第 8 章 7.7 已定义：读取 JSON 状态

    QString fileToOpen;

    const QStringList args = app.arguments();
    if (args.size() >= 2) {
        // 情况 A：命令行带了 md 文件路径（可能是“拖 md 到 exe”产生的）
        fileToOpen = args.at(1);
    } else {
        // 情况 B：双击 exe / 没有指定文件
        // 打开“上次看的文档”
        if (!w.appState().recentFiles.isEmpty()) {
            fileToOpen = w.appState().recentFiles.first(); // recentFiles[0] = 最近打开
        }
    }

    if (!fileToOpen.isEmpty()) {
        w.openMarkdownFile(fileToOpen); // 内部会自动恢复阅读位置（7.5）
    }

    w.show();
    return app.exec();
}
````

对应逻辑：

* **双击 exe**：`args.size() == 1`，没有参数 → 从 `recentFiles[0]` 找到最近打开的文档 → 调用 `openMarkdownFile()` → 根据 7.5 的逻辑自动恢复 `lastScrollY`。
* 如果是第一次使用，没有任何历史记录 → 不打开文档，只显示空白或“请拖动 .md 文件”的提示。

---

#### 7.10.2 “拖 md 到 exe” 打开文档

在 Windows 下，如果你把一个 `.md` 文件拖动到 exe 图标上：

* 系统会以这个文件路径作为命令行参数启动 exe，例如：

  ```text
  transparent_md_reader.exe "D:\docs\article.md"
  ```

所以上面 `main()` 里的：

```cpp
if (args.size() >= 2) {
    fileToOpen = args.at(1);
}
```

就覆盖了：

* “程序尚未运行时，把文件拖到 exe 上” 的情况：

  * 程序读 JSON → 载入状态 → `openMarkdownFile(fileToOpen)` → 自动恢复 `lastScrollY`。

这样你的需求 ① 和 ② 的“第一次打开”就都满足了。

---

#### 7.10.3 已经打开一个实例时，再拖 md 到 exe

你希望的是：

> 程序已经在运行，双击/拖其他 md 到 exe：
> 当前文档的阅读记录先保存，再打开新的 md，并恢复它的阅读位置。

这里就涉及 **单实例** 设计（只允许一个 MainWindow、一个进程工作），一般做法是：

1. 第一个实例运行时，创建一个“单实例锁”（如 `QLocalServer` / `QSharedMemory` / `QLockFile` 等）；
2. 后续再启动的进程发现锁已存在，不再创建窗口，而是把“要打开的文件路径”通过 IPC（本机 socket 等）发给第一个实例，然后立即退出；
3. 第一个实例收到“新文件路径”后：

   * 先 `captureScrollPosition()` 保存当前文档状态；
   * 再 `openMarkdownFile(newPath)` 打开新文档并恢复滚动位置。

流程大致是：

```cpp
// 伪代码结构，只说明逻辑：
bool isAnotherInstanceRunning();
void sendOpenFileRequestToPrimary(const QString &path);

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QString fileToOpen;
    const QStringList args = app.arguments();
    if (args.size() >= 2)
        fileToOpen = args.at(1);

    if (isAnotherInstanceRunning()) {
        if (!fileToOpen.isEmpty()) {
            sendOpenFileRequestToPrimary(fileToOpen);
        }
        return 0; // 这个进程直接退出，由主实例处理
    }

    MainWindow w;
    w.loadAppState();

    if (!fileToOpen.isEmpty()) {
        w.openMarkdownFile(fileToOpen);
    } else if (!w.appState().recentFiles.isEmpty()) {
        w.openMarkdownFile(w.appState().recentFiles.first());
    }

    w.show();
    return app.exec();
}
```

在主实例里，实现一个槽函数接收“打开文件请求”：

```cpp
// 例如槽函数：void MainWindow::onOpenFileRequestedFromSecondary(const QString &path)

void MainWindow::onOpenFileRequestedFromSecondary(const QString &path)
{
    // 1. 保存当前文档阅读位置
    captureScrollPosition(); // 前面 7.4 的逻辑

    // 2. 打开新文档（会自动恢复它自己的 lastScrollY）
    openMarkdownFile(path);
}
```

这样，不管你是：

* 已运行 → 再拖文件到 exe
* 已运行 → 通过文件关联 “双击 .md” 打开

最终都会走到：**主实例收到文件路径 → 保存当前 → 打开新文档并恢复阅读位置**。

> 单实例 + IPC 的具体实现细节（用 QSharedMemory/QLocalServer/QtSingleApplication 等）可以单独在后面章节写，我这里只把和“阅读位置保存/恢复”相关的部分串起来。

---

#### 7.10.4 整体行为回顾

* **双击 exe（无参数）**

  * 读配置文件（JSON）
  * 若 `recentFiles` 非空 → 打开最近的那一个 → 自动恢复 `lastScrollY`。

* **程序没运行，拖 md 到 exe**

  * 启动时命令行带了 md 路径 → 直接打开这个文档 → 自动恢复它的 `lastScrollY`。

* **程序已运行，再拖 md 到 exe（单实例模式）**

  * 第二个进程检测到已有实例，向主实例发送“打开这个文件”的请求后退出；
  * 主实例：

    * 先 `captureScrollPosition()` 保存当前文档阅读状态；
    * 再 `openMarkdownFile(newPath)` 打开新文档并恢复它的阅读位置；
    * 更新 `recentFiles` 和 `perFile` 状态，写回 JSON。

这一整套行为已经完全兼容第 8 章前面的设计，只是把**启动入口 / 多次启动 / 拖到 exe 的细节**明确写出来，整体逻辑是闭环的。

```
```



# 8. 单实例机制（QLocalServer + QLocalSocket）

目标：整个系统只跑 **一个** 阅读器进程，所有 “再启动 / 双击其它 md / 拖 md 到 exe” 都交给这个主实例来处理。

核心技术栈（Qt 官方推荐 IPC 方式之一）：  

- 进程间通信：`QLocalServer` + `QLocalSocket`（本地 socket / named pipe）:contentReference[oaicite:0]{index=0}  
- 判定单实例：能 `listen()` 的就是主实例，连上了现有 server 的就是次实例（参考 Qt 社区的标准写法）:contentReference[oaicite:1]{index=1}  

---

## 8.1 设计目标

1. **保证只跑一个实例（主实例）**
   - 第一次启动：成为主实例，创建 `QLocalServer`，正常显示窗口、加载文档。
   - 再次启动：发现已有主实例 → 不再开启新窗口，转而把“要打开的文件路径”发送给主实例后退出。

2. **支持两种入口统一处理**
   - 双击 `.md`（文件关联）：次实例通过 socket 把该路径发给主实例，由主实例打开。
   - 把 `.md` 拖到 `exe`：同样走命令行参数 + socket 通知主实例。

3. **和第 8 章状态持久化配合**
   - 主实例在收到“打开文件”请求前，先保存当前文档的阅读位置；
   - 再用第 8 章的 `openMarkdownFile()` 自动恢复新文档阅读位置。

---

## 8.2 整体思路

单实例模式的典型流程（也是 Qt 官方/社区共识的套路）：:contentReference[oaicite:2]{index=2}  

1. 定义一个全局唯一的 **server 名**，比如：  
   `zhiz.transparent_md_reader.server`

2. 程序启动时：
   - 先创建一个 `QLocalSocket`，尝试 `connectToServer(serverName)`：  
     - 如果能连上 → 说明已经有主实例在监听；  
       - 把命令行里的 md 路径写到 socket 里；  
       - 退出当前进程。  
     - 如果连不上 → 没有主实例：  
       - 清理可能残留的上次崩溃遗留的 server；  
       - 创建 `QLocalServer`，调用 `listen(serverName)`；  
       - 成为主实例，正常创建 `MainWindow` 等。

3. 主实例中：
   - `QLocalServer::newConnection` 触发时，通过 `nextPendingConnection()` 拿到一个 `QLocalSocket`；:contentReference[oaicite:3]{index=3}  
   - 从这个 socket 读出“要打开的文件路径”；  
   - 先 `captureScrollPosition()` 保存当前文档位置；  
   - 再 `openMarkdownFile(path)` 打开新文档并恢复阅读位置。

---

## 8.3 唯一 server 名 & 辅助函数

### 8.3.1 server 名

建议用组织名 + 应用名，避免与其它程序冲突：

```cpp
// SingleInstance.h
#pragma once
#include <QString>

inline QString singleInstanceServerName()
{
    // 可根据实际 org/app 名来写
    return QStringLiteral("zhiz.transparent_md_reader.server");
}
````

---

## 8.4 主实例：QLocalServer 初始化

在 `MainWindow` 或单独的 `SingleInstanceManager` 中持有一个 `QLocalServer`：

```cpp
// SingleInstanceManager.h
#pragma once

#include <QObject>
#include <QLocalServer>
#include <QLocalSocket>

class SingleInstanceManager : public QObject
{
    Q_OBJECT
public:
    explicit SingleInstanceManager(QObject *parent = nullptr);

    // 作为主实例开始监听；返回是否监听成功
    bool startServer(const QString &serverName);

signals:
    // 收到次实例请求：打开文件
    void openFileRequested(const QString &filePath);

private slots:
    void onNewConnection();

private:
    QLocalServer m_server;
};
```

```cpp
// SingleInstanceManager.cpp
#include "SingleInstanceManager.h"
#include "SingleInstance.h"
#include <QFile>

SingleInstanceManager::SingleInstanceManager(QObject *parent)
    : QObject(parent)
{
    connect(&m_server, &QLocalServer::newConnection,
            this, &SingleInstanceManager::onNewConnection);
}

bool SingleInstanceManager::startServer(const QString &serverName)
{
    // 跨平台建议先 remove 一下，避免 Unix 上遗留 socket 文件阻止 listen:contentReference[oaicite:4]{index=4}
    QLocalServer::removeServer(serverName);

    if (!m_server.listen(serverName)) {
        // 监听失败，可能是权限/其它问题
        return false;
    }
    return true;
}

void SingleInstanceManager::onNewConnection()
{
    QLocalSocket *socket = m_server.nextPendingConnection();
    if (!socket)
        return;

    // 连接断开时自动删除
    socket->setParent(this);

    connect(socket, &QLocalSocket::readyRead, this, [this, socket]{
        QByteArray data = socket->readAll();
        const QString message = QString::fromUtf8(data).trimmed();
        if (!message.isEmpty()) {
            emit openFileRequested(message);
        }
        socket->disconnectFromServer();
    });
}
```

`QLocalServer::listen()` 会在本地创建一个可接受连接的 socket，`newConnection` 信号配合 `nextPendingConnection()` 是 Qt 官方推荐的用法。([Qt 文档][1])

---

## 8.5 次实例：QLocalSocket 连接并发送文件路径

次实例只做一件事：**连上 server，把命令行里收到的路径写过去。**

```cpp
// SingleInstanceClient.h
#pragma once

#include <QString>

bool notifyRunningInstance(const QString &serverName,
                           const QString &fileToOpen,
                           int timeoutMs = 1000);
```

```cpp
// SingleInstanceClient.cpp
#include "SingleInstanceClient.h"
#include <QLocalSocket>

bool notifyRunningInstance(const QString &serverName,
                           const QString &fileToOpen,
                           int timeoutMs)
{
    QLocalSocket socket;
    socket.connectToServer(serverName);

    if (!socket.waitForConnected(timeoutMs)) {
        // 连不上：认为主实例不存在
        return false;
    }

    const QByteArray data = fileToOpen.toUtf8();
    socket.write(data);
    socket.flush();  // 让数据尽快发送:contentReference[oaicite:6]{index=6}
    socket.waitForBytesWritten(timeoutMs);

    socket.disconnectFromServer();
    return true;
}
```

`QLocalSocket::connectToServer()` / `waitForConnected()` / `write()` / `flush()` / `waitForBytesWritten()` 是官方提供的本地 socket 用法，可确保小数据包可靠送达。([Qt 文档][2])

---

## 8.6 main() 中的单实例判定流程

把前面各章的启动逻辑整合起来：

```cpp
// main.cpp
#include <QApplication>
#include "MainWindow.h"
#include "SingleInstance.h"
#include "SingleInstanceManager.h"
#include "SingleInstanceClient.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    const QString serverName = singleInstanceServerName();
    const QStringList args   = app.arguments();
    QString fileFromCmd;

    if (args.size() >= 2) {
        fileFromCmd = args.at(1);
    }

    // 1. 先假设已经有主实例在运行：尝试用 QLocalSocket 通知它
    if (!fileFromCmd.isEmpty()) {
        if (notifyRunningInstance(serverName, fileFromCmd)) {
            // 通知成功，说明已有主实例处理这个请求，本进程退出
            return 0;
        }
    } else {
        // 没文件参数，依然可以用 notifyRunningInstance 测试一下是否有主实例存在
        // 不发送路径，只测试连接成功与否：
        if (notifyRunningInstance(serverName, QString())) {
            return 0;
        }
    }

    // 2. 走到这里，说明没连上任何 server → 自己成为主实例
    MainWindow w;

    // 2.1 加载应用状态（第 8 章）
    w.loadAppState();

    // 2.2 启动单实例 server
    auto *singleMgr = new SingleInstanceManager(&w);
    if (!singleMgr->startServer(serverName)) {
        // 监听失败，仍继续运行，只是无法拦截次实例
    }

    // 2.3 主实例收到“打开文件”请求时的处理
    QObject::connect(singleMgr, &SingleInstanceManager::openFileRequested,
                     &w, &MainWindow::onOpenFileRequestedFromSecondary);

    // 2.4 首次打开的文档（没有参数 = 打开最近文档；有参数 = 打开参数文档）
    if (!fileFromCmd.isEmpty()) {
        w.openMarkdownFile(fileFromCmd);
    } else if (!w.appState().recentFiles.isEmpty()) {
        w.openMarkdownFile(w.appState().recentFiles.first());
    }

    w.show();
    return app.exec();
}
```

这个流程对应：

* 有参数且主实例存在 → `notifyRunningInstance()` 成功 → 次实例退出；
* 无参数但主实例存在 → 同样可以用空消息测试连接 → 直接退出；
* 无主实例 → 走 `startServer()`，当前进程成为主实例。

---

## 8.7 MainWindow：收到次实例请求时如何切换文档

结合第 8 章的状态持久化，我们在 `MainWindow` 中增加一个槽函数：

```cpp
// MainWindow.h
public slots:
    void onOpenFileRequestedFromSecondary(const QString &path);
```

```cpp
// MainWindow.cpp
#include "MainWindow.h"

void MainWindow::onOpenFileRequestedFromSecondary(const QString &path)
{
    if (path.isEmpty())
        return;

    // 1. 保存当前文档的阅读位置（第 8 章 8.4）
    captureScrollPosition();

    // 2. 打开新文档（内部会自动恢复它自己的 lastScrollY）
    openMarkdownFile(path);

    // 3. 让窗口出现在前台（可选）
    this->show();
    this->raise();
    this->activateWindow();
}
```

这样可以满足你之前提到的场景：

> 已经打开一个文档，但是又拖动其它 md 到 exe：
> 它会把当前文档的阅读记录保存，然后读取拖动的 md 的阅读记录并打开。

---

## 8.8 与文件关联 / 拖放到 exe 的配合

在 Windows 上：

* 如果你把 `.md` 文件关联到 `transparent_md_reader.exe`，双击该 `.md` 时，系统会用类似：

  ```text
  transparent_md_reader.exe "D:\docs\article.md"
  ```

  启动程序（带文件路径参数）。

* 如果你把 `.md` 拖到 exe 文件图标上，效果一样：也是“带参数启动”。

因此：

* **程序未运行时**：

  * 由当前这个 `main()` 作为“第一次启动”；
  * 会直接 `openMarkdownFile(path)` 打开该文档并恢复进度。

* **程序已运行（主实例已存在）时**：

  * 新进程会先 `notifyRunningInstance()`，把路径发送到主实例；
  * 主实例执行 `onOpenFileRequestedFromSecondary()`：

    * 保存当前文档位置；
    * 打开新文档并恢复阅读进度；
  * 新进程立即退出。

---

## 8.9 清理遗留 server 的注意事项

* 在 Unix/Linux 上，`QLocalServer` 通常通过文件系统里的 socket 文件实现；异常退出有可能留下这个文件阻止再次 `listen()`。Qt 官方文档和实践中都推荐在 `listen()` 前调用 `QLocalServer::removeServer(name)` 清理遗留 socket。([Qt 文档][1])
* 在 Windows 上，`QLocalServer` 通过 named pipe 实现，`removeServer()` 同样是推荐用法，用来清理逻辑上的残留。

我们在 `startServer()` 中已经做了这一点。

---

## 8.10 与现有各章的关系

* 第 5 章（Markdown 渲染）：
  单实例只负责“谁来打开哪个文档”，具体打开后如何渲染仍由第 5 章逻辑完成。

* 第 6 章（主题系统）和第 10 章（系统托盘）：
  单实例保证只有一个托盘图标 / 一个设置中心，避免多个实例抢托盘。

* 第 7 章（文档状态持久化）中的链接跳转场景：
  链接在同一进程内部跳转时，依赖第 7 章保存 / 恢复阅读位置。

* 本章（第 8 章：单实例机制）与第 7 章（状态持久化）：
  单实例机制和状态持久化模块配合，使得：
  - 启动时可以自动恢复“上次阅读的文档 + 阅读位置”；
  - 新打开的 md（通过文件关联 / 拖到 exe）同样可以无缝保存 / 恢复阅读进度。


---

## 8.11 小结

1. 使用 `QLocalServer + QLocalSocket` 实现单实例，是当前 Qt 官方和社区里非常常见、稳定的一种方案，专门用来做本机进程间通信。([Qt 文档][1])
2. 主实例负责 `listen()` 并处理 `newConnection()`，次实例只负责连接、发送“打开文件”的请求然后退出。
3. 整个机制完整覆盖了：

   * 双击 exe → 自动打开上次阅读的文档；
   * 双击 `.md` / 拖 `.md` 到 exe → 打开对应文件并恢复进度；
   * 程序已运行时，再双击/拖拽 → 不产生第二个窗口，而是复用主实例。

下一章可以专门写 **系统托盘集成（QSystemTrayIcon + 菜单）**，把主题切换、最近文件列表、退出等入口放到托盘里，和这套单实例机制一起构成完整的“常驻工具”体验。

```
```

[1]: https://doc.qt.io/qt-6/qlocalserver.html?utm_source=chatgpt.com "QLocalServer Class | Qt Network | Qt 6.10.0"
[2]: https://doc.qt.io/qt-6/qlocalsocket.html?utm_source=chatgpt.com "QLocalSocket Class | Qt Network | Qt 6.10.0"


# 9. 拖放文件到阅读窗口（dragEnterEvent / dropEvent）

本章只解决一件事：

> **把 `.md` 文件直接拖到已经打开的阅读窗口上，也能切换文档并恢复阅读位置。**

和 “拖 md 到 exe 图标 / 双击 md” 不同，这里是 **拖到窗口本身**，用的是 `dragEnterEvent()` + `dropEvent()` 机制。Qt 官方推荐做法就是：  
- `setAcceptDrops(true)`  
- 重写 `dragEnterEvent()` / `dropEvent()`，检查 MIME，再处理文件。  

---

## 9.1 设计目标

1. 在主窗口（阅读窗口）上，允许拖入 `.md` 文件。
2. **拖入 .md 时行为：**
   - 先保存当前文档的阅读位置（第 8 章逻辑）；
   - 再打开拖入的 `.md` 文件；
   - 若已有该文档阅读记录，则自动恢复阅读位置。
3. 只接受 `.md` 文件，其它类型一律忽略（不打扰你的工作流）。

> 注意：  
> 窗口处于 🔒 锁定状态时，整个窗口是“鼠标穿透”的（`WindowTransparentForInput`），此时本来就不收鼠标/拖放事件。  
> 所以**拖放文件只在未锁定或 Ctrl 临时解锁时有效**，这跟你“锁定时只负责显示、不打扰工作”的设计是统一的。

---

## 9.2 启用拖放支持

在 Qt 中，要让一个窗口能接收拖放事件，必须：

1. `widget->setAcceptDrops(true);`
2. 重写 `dragEnterEvent(QDragEnterEvent*)`
3. 重写 `dropEvent(QDropEvent*)`  

在 `MainWindow` 构造函数里：

```cpp
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // ... 原有初始化（WebEngine、Backend、托盘等）

    setAcceptDrops(true);  // 允许本窗口接收拖放
}
````

---

## 9.3 dragEnterEvent：只接受 .md 文件

当你把文件拖到窗口上方时，Qt 会先发 `dragEnterEvent`，你需要：

* 检查 `event->mimeData()` 是否包含 URL 列表；
* 检查是否至少有一个后缀为 `.md` 的文件；
* 如果有，就 `acceptProposedAction()`，否则 `ignore()`。

```cpp
#include <QDragEnterEvent>
#include <QMimeData>
#include <QUrl>
#include <QFileInfo>

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    const QMimeData *mime = event->mimeData();
    if (!mime->hasUrls()) {
        event->ignore();
        return;
    }

    // 检查是否至少有一个 .md 文件
    bool hasMd = false;
    const QList<QUrl> urls = mime->urls();
    for (const QUrl &url : urls) {
        if (!url.isLocalFile())
            continue;
        QFileInfo info(url.toLocalFile());
        if (info.suffix().compare("md", Qt::CaseInsensitive) == 0) {
            hasMd = true;
            break;
        }
    }

    if (hasMd) {
        event->acceptProposedAction();  // 告诉系统：我愿意接收这个拖放
    } else {
        event->ignore();
    }
}
```

---

## 9.4 dropEvent：保存当前进度 → 打开新文档

`dropEvent` 触发时，你需要：

1. 拿到第一个 `.md` 文件路径；
2. 调用我们前面已经有的逻辑：

   * `captureScrollPosition()` 保存当前文档的 scrollY；
   * `openMarkdownFile(newPath)` 打开新文档并恢复阅读位置；
   * 更新最近文件列表（第 8 章）。

```cpp
#include <QDropEvent>
#include <QMimeData>

void MainWindow::dropEvent(QDropEvent *event)
{
    const QMimeData *mime = event->mimeData();
    if (!mime->hasUrls()) {
        event->ignore();
        return;
    }

    QString targetPath;

    const QList<QUrl> urls = mime->urls();
    for (const QUrl &url : urls) {
        if (!url.isLocalFile())
            continue;

        const QString local = url.toLocalFile();
        QFileInfo info(local);
        if (info.suffix().compare("md", Qt::CaseInsensitive) == 0) {
            targetPath = info.absoluteFilePath();
            break;  // 只取第一个 .md
        }
    }

    if (targetPath.isEmpty()) {
        event->ignore();
        return;
    }

    event->acceptProposedAction();

    // 1. 保存当前文档阅读位置（第 8 章中已有实现）
    captureScrollPosition();

    // 2. 打开新文档（内部会自动恢复该文档的 lastScrollY）
    openMarkdownFile(targetPath);

    // 3. 窗口置前（可选）
    this->show();
    this->raise();
    this->activateWindow();
}
```

> 如果你希望一次拖多个 `.md` 时打开第一个，其余忽略，上面的写法已经满足。
> 如果以后想支持“多标签/多窗口”，可以在这里改成一次处理多个路径。

---

## 9.5 与单实例 / 文件关联的关系

现在文件打开入口有三类：

1. **命令行参数 / 文件关联 / 拖 md 到 exe** → 由 `main()` + 第 9 章单实例 + IPC 处理；
2. **内部链接**（第 7 章 `openMarkdownLink`） → 在同一实例内切换；
3. **把 md 拖到阅读窗口** → 本章的 `dragEnterEvent` / `dropEvent` 路径；

三条路径最终都汇总到一套打开逻辑：

```cpp
navigateTo(path)  // 或直接 openMarkdownFile(path)
```

并且在切换前都会调用：

```cpp
captureScrollPosition(); // 保存当前文档的阅读位置
```

所以只要你保证这几处都走 `captureScrollPosition()` + `openMarkdownFile()`，行为在所有入口上是一致的。

---

## 9.6 与锁定 / Ctrl 模式的兼容性

* 🔒 **锁定状态（WindowTransparentForInput=true）**：

  * 窗口不接收鼠标事件，自然也收不到拖放事件；
  * 此时拖文件实际上是拖到底下的应用窗口上，阅读器不会响应；
  * 这符合“锁定时只当背景看，不做交互”的设计。

* 🔓 **未锁定 或 Ctrl 按住临时解锁**：

  * 窗口恢复正常接收鼠标事件；
  * `dragEnterEvent` / `dropEvent` 正常工作，可以拖入 `.md` 切换文档。

你不需要为了拖放额外改锁定逻辑，Qt 本身会根据窗口当前的输入 flag 来决定是否派发拖放事件。

---

## 9.7 小结

1. 拖放到阅读窗口用的是 Qt 标准机制：

   * `setAcceptDrops(true)`
   * 重写 `dragEnterEvent()` 和 `dropEvent()` 来控制接受什么。

2. 行为规则：

   * 只接受扩展名为 `.md` 的本地文件；
   * 拖入后先保存当前文档阅读位置，再打开新文档并恢复它自己的阅读进度。

3. 与第 7 章（文档状态持久化）和第 8 章（单实例机制）一起，形成统一的“打开文档入口”体系：

   * 双击 exe / 双击 md / 拖 md 到 exe；
   * 拖 md 到窗口；
   * 点击内部链接；
     最终都用一套逻辑切换文档和保存/恢复阅读位置。
     以上入口最终都复用同一套 captureScrollPosition() + openMarkdownFile() / navigateTo() 逻辑。

```
```

# 10. 系统托盘设计（QSystemTrayIcon）

本章定义透明 Markdown 阅读器的 **系统托盘整体方案**，包括：

- 使用 `QSystemTrayIcon` 创建托盘图标与菜单  
- 菜单结构设计（打开文件 / 最近文件 / 主题 / 内容透明度 / 窗口控制 / 开机自启 / 调试日志 / 退出）  
- 托盘点击行为（左键显示/隐藏窗口、右键打开菜单）  
- 与第 6 章“主题系统”和第 8 章“历史记录 + 阅读进度”、日志系统的对接  

Qt 官方推荐通过 `QSystemTrayIcon` + `QMenu` 组合实现系统托盘图标与菜单，并通过 `activated()` 信号区分单击、双击、右键等行为。  

---

## 10.1 设计目标与原则

1. **阅读窗口只负责阅读**  
   - 阅读窗口本身只保留 `X` 和 `🔒` 按钮。:contentReference[oaicite:1]{index=1}  
   - 所有设置类功能（主题、透明度、最近文件、开机自启、调试日志等）集中放在托盘菜单。

2. **托盘是控制中枢**  
   - 托盘菜单项：  
     - 打开 Markdown 文件…  
     - 最近打开  
     - 主题（深色/浅色/纸张…）  
     - 内容透明度预设（低/中/高）  
     - 显示/隐藏阅读窗口  
     - 始终置顶  
     - 开机自启（Windows）  
     - 调试日志（开/关）  
     - 退出  

3. **行为简单统一**  
   - 右键托盘图标 → 显示菜单（系统 + Qt 默认行为）。  
   - 左键单击或双击托盘图标 → 切换阅读窗口 显示/隐藏。

---

## 10.2 QSystemTrayIcon 基础结构

### 10.2.1 成员与初始化

```cpp
// MainWindow.h（示例）
#include <QSystemTrayIcon>
#include <QMenu>

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    void createTrayIcon();
    void createTrayMenu();
    void updateTrayRecentFilesMenu();

private slots:
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void onActionOpenFile();
    void onActionToggleWindowVisible();
    void onActionQuit();

    void applyTheme(const QString &themeName);        // 第 6 章
    void applyContentOpacity(double opacity);         // 第 6 章
    void setAutoStartEnabled(bool enabled);           // 10.9
    void setDebugLoggingEnabled(bool enabled);        // 10.10
    void setFileLoggingEnabled(bool enabled);         // 10.11

private:
    QSystemTrayIcon *m_trayIcon = nullptr;
    QMenu           *m_trayMenu = nullptr;

    QMenu           *m_recentMenu = nullptr;
    QMenu           *m_themeMenu = nullptr;
    QMenu           *m_opacityMenu = nullptr;

    QAction         *m_actToggleVisible = nullptr;
    QAction         *m_actAlwaysOnTop = nullptr;
    QAction         *m_actAutostart = nullptr;
    QAction         *m_actDebugLogging = nullptr;
    QAction         *m_actFileLogging = nullptr;
};
````

构造函数中初始化托盘（先检查系统是否支持系统托盘）：

```cpp
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // ... 其它初始化（WebEngine、Backend、透明窗口等）

    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        createTrayIcon();
    } else {
        // 可选：提示当前桌面环境不支持系统托盘
    }
}
```

### 10.2.2 创建托盘图标与菜单

```cpp
void MainWindow::createTrayIcon()
{
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(QIcon(":/icons/app_tray_icon.png"));
    m_trayIcon->setToolTip(tr("Transparent MD Reader"));

    createTrayMenu();                        // 构建 context menu
    m_trayIcon->setContextMenu(m_trayMenu);  // 右键菜单

    connect(m_trayIcon, &QSystemTrayIcon::activated,
            this, &MainWindow::onTrayIconActivated);

    m_trayIcon->show();
}
```

---

## 10.3 菜单结构设计

托盘菜单从上到下：

1. 打开 Markdown 文件…
2. 最近打开（子菜单）
3. 分隔线
4. 主题（子菜单：深色透明 / 浅色透明 / 纸张…）
5. 内容透明度（子菜单：低 / 中 / 高）
6. 分隔线
7. 显示/隐藏阅读窗口（checkable）
8. 始终置顶（checkable）
9. 开机自启（Windows，checkable）
10. 调试日志（checkable，控制 QLoggingCategory filter）
11. 写入日志文件（checkable，控制 qInstallMessageHandler）
12. 分隔线
10. 退出

### 10.3.1 创建菜单

```cpp
void MainWindow::createTrayMenu()
{
    m_trayMenu = new QMenu(this);

    // 1. 打开文件
    QAction *actOpenFile = m_trayMenu->addAction(tr("打开 Markdown 文件(&O)..."));
    connect(actOpenFile, &QAction::triggered,
            this, &MainWindow::onActionOpenFile);

    // 2. 最近打开
    m_recentMenu = m_trayMenu->addMenu(tr("最近打开(&R)"));
    updateTrayRecentFilesMenu(); // 第 8 章的 recentFiles

    m_trayMenu->addSeparator();

    // 3. 主题
    m_themeMenu = m_trayMenu->addMenu(tr("主题(&T)"));
    QAction *actThemeDark  = m_themeMenu->addAction(tr("深色透明"));
    QAction *actThemeLight = m_themeMenu->addAction(tr("浅色透明"));
    QAction *actThemePaper = m_themeMenu->addAction(tr("纸张风格"));

    connect(actThemeDark,  &QAction::triggered,
            this, [this]{ applyTheme(QStringLiteral("theme-dark")); });
    connect(actThemeLight, &QAction::triggered,
            this, [this]{ applyTheme(QStringLiteral("theme-light")); });
    connect(actThemePaper, &QAction::triggered,
            this, [this]{ applyTheme(QStringLiteral("theme-paper")); });

    // 4. 内容透明度
    m_opacityMenu = m_trayMenu->addMenu(tr("内容透明度(&P)"));
    QAction *actOpacityLow    = m_opacityMenu->addAction(tr("低（更不透明）"));
    QAction *actOpacityMedium = m_opacityMenu->addAction(tr("中"));
    QAction *actOpacityHigh   = m_opacityMenu->addAction(tr("高（更透明）"));

    connect(actOpacityLow,    &QAction::triggered,
            this, [this]{ applyContentOpacity(0.95); });
    connect(actOpacityMedium, &QAction::triggered,
            this, [this]{ applyContentOpacity(0.85); });
    connect(actOpacityHigh,   &QAction::triggered,
            this, [this]{ applyContentOpacity(0.70); });

    m_trayMenu->addSeparator();

    // 5. 显示 / 隐藏阅读窗口
    m_actToggleVisible = m_trayMenu->addAction(tr("显示阅读窗口"));
    m_actToggleVisible->setCheckable(true);
    m_actToggleVisible->setChecked(true);
    connect(m_actToggleVisible, &QAction::triggered,
            this, &MainWindow::onActionToggleWindowVisible);

    // 6. 始终置顶
    m_actAlwaysOnTop = m_trayMenu->addAction(tr("始终置顶"));
    m_actAlwaysOnTop->setCheckable(true);
    m_actAlwaysOnTop->setChecked(true); // 默认置顶，可从配置恢复

    connect(m_actAlwaysOnTop, &QAction::toggled,
            this, [this](bool on) {
                Qt::WindowFlags flags = windowFlags();
                if (on)
                    flags |= Qt::WindowStaysOnTopHint;
                else
                    flags &= ~Qt::WindowStaysOnTopHint;
                setWindowFlags(flags);
                show(); // 重新 show 以应用新 flags
            });

    // 7. 开机自启（Windows）
#ifdef Q_OS_WIN
    m_actAutostart = m_trayMenu->addAction(tr("开机自启"));
    m_actAutostart->setCheckable(true);
    m_actAutostart->setChecked(isAutoStartEnabled());
    connect(m_actAutostart, &QAction::toggled,
            this, &MainWindow::setAutoStartEnabled);
#endif

    // 8. 调试日志开关
    m_actDebugLogging = m_trayMenu->addAction(tr("调试日志(&D)"));
    m_actDebugLogging->setCheckable(true);
    m_actDebugLogging->setChecked(m_state.debugLoggingEnabled);
    connect(m_actDebugLogging, &QAction::toggled,
            this, &MainWindow::setDebugLoggingEnabled);

    // 9. 写入日志文件
    m_actFileLogging = m_trayMenu->addAction(tr("写入日志文件(&L)"));
    m_actFileLogging->setCheckable(true);
    m_actFileLogging->setChecked(m_state.fileLoggingEnabled);
    connect(m_actFileLogging, &QAction::toggled,
            this, &MainWindow::setFileLoggingEnabled);

    m_trayMenu->addSeparator();

    // 10. 退出
    QAction *actQuit = m_trayMenu->addAction(tr("退出(&Q)"));
    connect(actQuit, &QAction::triggered,
            this, &MainWindow::onActionQuit);
}
```

---

## 10.4 最近文件菜单的动态构建

依赖第 8 章的 `AppState::recentFiles`：

```cpp
void MainWindow::updateTrayRecentFilesMenu()
{
    if (!m_recentMenu)
        return;

    m_recentMenu->clear();

    const auto recent = m_state.recentFiles;
    if (recent.isEmpty()) {
        QAction *placeholder = m_recentMenu->addAction(tr("（无最近文件）"));
        placeholder->setEnabled(false);
        return;
    }

    for (const QString &path : recent) {
        QFileInfo fi(path);
        QAction *act = m_recentMenu->addAction(fi.fileName());
        act->setData(path);
        connect(act, &QAction::triggered, this, [this, act]{
            const QString filePath = act->data().toString();
            navigateTo(filePath); // 内部保存当前进度 + 打开新文档 + 恢复进度
        });
    }
}
```

`updateTrayRecentFilesMenu()` 在每次更新 `recentFiles` 后调用一次。

---

## 10.5 托盘图标点击行为（activated 信号）

Qt 通过 `QSystemTrayIcon::activated(QSystemTrayIcon::ActivationReason)` 提供托盘事件：Trigger、DoubleClick、Context 等。

我们的行为定义：

* 左键单击 / 双击 → 切换阅读窗口显示/隐藏
* 右键 → 系统自动弹出 contextMenu，不额外处理

```cpp
void MainWindow::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason) {
    case QSystemTrayIcon::Trigger:      // 左键单击
    case QSystemTrayIcon::DoubleClick:  // 双击
        onActionToggleWindowVisible();
        break;
    case QSystemTrayIcon::Context:
        // 右键：Qt 会自动显示 contextMenu
        break;
    case QSystemTrayIcon::MiddleClick:
    default:
        break;
    }
}
```

### 10.5.1 显示/隐藏阅读窗口

```cpp
void MainWindow::onActionToggleWindowVisible()
{
    const bool visibleNow = this->isVisible();
    if (visibleNow) {
        this->hide();
        if (m_actToggleVisible)
            m_actToggleVisible->setChecked(false);
    } else {
        this->show();
        this->raise();
        this->activateWindow();
        if (m_actToggleVisible)
            m_actToggleVisible->setChecked(true);
    }
}
```

---

## 10.6 打开文件与退出行为

### 10.6.1 打开 Markdown 文件…

```cpp
void MainWindow::onActionOpenFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("选择 Markdown 文件"),
        QString(),
        tr("Markdown 文件 (*.md);;所有文件 (*.*)")
    );

    if (path.isEmpty())
        return;

    navigateTo(path); // 统一入口：保存当前进度 + 打开 + 恢复
}
```

### 10.6.2 退出程序

```cpp
void MainWindow::onActionQuit()
{
    captureScrollPosition(); // 第 8 章
    saveAppState();          // 第 8 章

    qApp->quit();
}
```

阅读窗口右上角 `X` 同样调用 `onActionQuit()`，保证行为统一。

---

## 10.7 与其他章节的关系

1. **第 3 / 4 章（透明窗体 + 锁定 / Ctrl 临时解锁）**

   * 托盘菜单只控制窗口可见性、置顶，不改锁定状态。锁定仍由窗口内的 `🔒` 和键盘 Ctrl 控制。

2. **第 6 章（主题系统）**

   * 本章的主题/透明度菜单是对第 6 章 `applyTheme()` / `applyContentOpacity()` 的具体入口。

3. **第 7 章（历史记录 + 阅读进度）**

   * “最近打开”菜单完全依赖 `AppState::recentFiles` 和 `navigateTo()`，保证所有入口切换文档时都能保存/恢复阅读位置。

4. **第 9–10 章（单实例 + 拖放）**

   * 不管是：

     * 双击 exe / 双击 md
     * 拖 md 到 exe
     * 拖 md 到阅读窗口
     * 从托盘“最近打开”选择
     * 点击内部链接（内部链接跳转逻辑复用 navigateTo() / captureScrollPosition()）
       都汇总到同一套 `captureScrollPosition() + openMarkdownFile()` 流程。

---

## 10.8 小结

* 使用 `QSystemTrayIcon + QMenu` 实现托盘控制中心，是 Qt 官方示例和文档推荐的模式。
* 阅读窗口保持极简，系统托盘承担“设置 + 入口 + 退出”的角色。

---

## 10.9 开机自启（Windows）

本节只针对 Windows，提供一个“开机自启”勾选项，让用户决定是否随登录自动启动阅读器。

### 10.9.1 官方路径（Run 注册表）

Windows 官方文档说明：可以通过 `Run` 注册表键在用户登录时自动启动程序：

* 当前用户：`HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run`
* 所有用户：`HKEY_LOCAL_MACHINE\Software\Microsoft\Windows\CurrentVersion\Run`

本项目只使用 **当前用户** 的 `HKCU` 路径，避免管理员权限。

### 10.9.2 实现代码

```cpp
#ifdef Q_OS_WIN
#include <QSettings>
#include <QDir>

static const char *kRunKeyPath =
    "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const char *kRunValueName = "TransparentMdReader";

bool MainWindow::isAutoStartEnabled() const
{
    QSettings reg(kRunKeyPath, QSettings::NativeFormat);
    return reg.value(kRunValueName).isValid();
}

void MainWindow::setAutoStartEnabled(bool enabled)
{
    QSettings reg(kRunKeyPath, QSettings::NativeFormat);
    if (enabled) {
        const QString exe = QDir::toNativeSeparators(
            QCoreApplication::applicationFilePath());
        reg.setValue(kRunValueName, exe);
    } else {
        reg.remove(kRunValueName);
    }

    m_state.autoStartEnabled = enabled;
    m_dirty = true;
}
#endif
```

托盘菜单的 `m_actAutostart` 勾选状态通过 `isAutoStartEnabled()` 初始化，每次切换时调用 `setAutoStartEnabled()`，并同步写入 JSON 配置。

---

## 10.10 调试日志开关（QLoggingCategory）

Qt 推荐使用 `QLoggingCategory` + 过滤规则控制日志输出级别，而不是在代码里大量使用宏 if。

### 10.10.1 日志分类

定义一个 logging category，用来标记本程序自己的日志：

```cpp
// logging.h
#include <QLoggingCategory>
Q_DECLARE_LOGGING_CATEGORY(lcReader)

// logging.cpp
#include "logging.h"
Q_LOGGING_CATEGORY(lcReader, "reader")
```

使用时：

```cpp
#include "logging.h"

void MainWindow::openMarkdownFile(const QString &path)
{
    qCDebug(lcReader) << "Open markdown file:" << path;
    ...
}
```

### 10.10.2 通过 FilterRules 控制开关

Qt 官方文档与 KDE 指南都建议通过 `QLoggingCategory::setFilterRules()` 在运行时调整哪些 category 的 debug/info 输出开启。

示例：默认关闭所有 debug，只开 `reader`：

```cpp
void MainWindow::setDebugLoggingEnabled(bool enabled)
{
    if (enabled) {
        // 开启 reader 的 debug，保持其它 category 默认行为
        QLoggingCategory::setFilterRules(
            QStringLiteral("reader.debug=true\n")
        );
    } else {
        // 关闭 reader 的 debug（warning/critical 仍然输出）
        QLoggingCategory::setFilterRules(
            QStringLiteral("reader.debug=false\n")
        );
    }

    m_state.debugLoggingEnabled = enabled;
    m_dirty = true;
}
```

在托盘菜单中，“调试日志(&D)” 勾选项的切换逻辑已经在 10.3 中连接到 `setDebugLoggingEnabled()`。

> 说明：
>
> * 这里简单地只控制 `reader.debug`，不动 Qt 内部其它 category；
> * 如果将来需要更细的控制，可以扩展为多条规则（例如 `*.debug=false` + 单独开启若干 category）。

---

## 10.11 日志写入文件与轮转（qInstallMessageHandler）

本节在 10.10 的基础上，把 Qt 日志（包括 `qCDebug(lcReader)`、`qWarning()` 等）写入本地 log 文件，并做一个简单的**按文件大小轮转**，避免日志无限增长。

Qt 官方 `QtLogging` 文档建议使用 `qInstallMessageHandler()` 安装一个全局消息处理函数，将所有日志转发到自定义逻辑（例如写文件），同时可选择调用默认处理器。

### 10.11.1 日志目录与文件命名

约定：

* 配置目录同第 8 章使用的 `detectConfigPath()` 所在目录：

  * 安装版：`QStandardPaths::AppConfigLocation` 下的应用目录
  * 便携版：exe 同目录
* 日志目录：`<configDir>/logs/`
* 当前日志文件：`reader.log`
* 轮转文件：`reader.log.1`（只保留一个历史文件，简单够用）

```cpp
static QString logDirPath()
{
    const QString baseConfig = QFileInfo(detectConfigPath()).absolutePath();
    QDir dir(baseConfig);
    dir.mkpath("logs");
    return dir.filePath("logs");
}

static QString logFilePath()
{
    QDir dir(logDirPath());
    return dir.filePath("reader.log");
}

static QString rotatedLogFilePath()
{
    QDir dir(logDirPath());
    return dir.filePath("reader.log.1");
}
```

### 10.11.2 简单的按大小轮转策略

Qt 本身不内置日志轮转 API，官方和社区建议是：自己检查文件大小并移动/删除旧文件，或者使用外部日志框架。

这里使用一个简单策略：

* 每次写日志前检查当前 log 文件大小：

  * 如果超过 `kMaxLogSizeBytes`（例如 5 MB）：

    * 若存在 `reader.log.1` → 删除
    * 把 `reader.log` 重命名为 `reader.log.1`
* 新日志永远写入 `reader.log`

```cpp
static const qint64 kMaxLogSizeBytes = 5 * 1024 * 1024; // 5MB

static void rotateLogFileIfNeeded()
{
    QFile f(logFilePath());
    if (!f.exists())
        return;

    if (f.size() <= kMaxLogSizeBytes)
        return;

    f.close();

    QFile old(rotatedLogFilePath());
    if (old.exists())
        old.remove();

    QFile::rename(logFilePath(), rotatedLogFilePath());
}
```

### 10.11.3 自定义 message handler

Qt 官方文档给出了一个写入本地文件的 message handler 示例。

在本项目中实现一个线程安全的 handler，写 UTF-8 文本，格式包含时间、类型、category 和消息内容：

```cpp
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QMutex>
#include <QMutexLocker>

static QtMessageHandler g_prevHandler = nullptr;
static bool g_fileLoggingEnabled = false;

void installFileLogger(bool enabled)
{
    g_fileLoggingEnabled = enabled;
    if (enabled) {
        if (!g_prevHandler) {
            g_prevHandler = qInstallMessageHandler([](
                QtMsgType type,
                const QMessageLogContext &ctx,
                const QString &msg)
            {
                if (g_fileLoggingEnabled) {
                    static QMutex mutex;
                    QMutexLocker locker(&mutex);

                    rotateLogFileIfNeeded();

                    QFile file(logFilePath());
                    if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
                        QTextStream out(&file);
                        out.setCodec("UTF-8"); // 保证中文正常记录

                        QString level;
                        switch (type) {
                        case QtDebugMsg:    level = "DEBUG"; break;
                        case QtInfoMsg:     level = "INFO";  break;
                        case QtWarningMsg:  level = "WARN";  break;
                        case QtCriticalMsg: level = "CRIT";  break;
                        case QtFatalMsg:    level = "FATAL"; break;
                        }

                        const QString ts =
                            QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");

                        QString category = QString::fromUtf8(ctx.category);
                        if (category.isEmpty())
                            category = QStringLiteral("default");

                        out << ts << " [" << level << "] "
                            << "(" << category << ") "
                            << msg << '\n';
                    }
                }

                // 调用之前的 handler，保持控制台 / 调试输出行为
                if (g_prevHandler) {
                    g_prevHandler(type, ctx, msg);
                }
            });
        }
    } else {
        // 只关闭写文件，保留 handler，本次之后 handler 内不会再写文件
        g_fileLoggingEnabled = false;
    }
}
```

说明：

* `g_prevHandler` 保存原始 message handler，安装自定义 handler 后仍然调用原 handler，这样不会破坏默认的控制台输出行为。
* `g_fileLoggingEnabled` 用来控制是否写入文件，便于通过托盘菜单关闭文件日志但保留 handler（避免频繁安装/卸载）。
* 使用 `QMutex` 确保多线程下写文件安全。

### 10.11.4 与托盘“写入日志文件(&L)”联动

在 `MainWindow` 中封装一层：

```cpp
void MainWindow::setFileLoggingEnabled(bool enabled)
{
    installFileLogger(enabled);
    m_state.fileLoggingEnabled = enabled;
    m_dirty = true;
}
```

* 程序启动时，在 `loadAppState()` 后调用一次 `setFileLoggingEnabled(m_state.fileLoggingEnabled)`，配合 `installFileLogger()` 完成初始化。
* 如果用户关闭“写入日志文件”，日志仍然可以照常通过 console / debugger 看到，只是不再写磁盘。

### 10.11.5 日志级别策略建议

结合 10.10 + 10.11，建议策略：

* **无论调试日志是否开启**，`qWarning` / `qCritical` / `qFatal` 始终写入日志文件，便于排查问题。
* “调试日志(&D)” 开关控制 `reader.debug`：

  * 关闭时：不写 debug 信息，只记录 info / warning / error；
  * 开启时：`qCDebug(lcReader)` 也会输出到文件。
* 若需要减少日志量，可以在 message handler 中根据 `type` 和 `ctx.category` 再做一次筛选，例如只记录 `reader` 相关内容。

---

## 10.12 小结（日志部分）

* 调试级别开关：用 `QLoggingCategory + setFilterRules()`，是 Qt 官方推荐的做法。
* 写入文件：用 `qInstallMessageHandler()` 安装全局 handler，将日志写到本地 UTF-8 文本文件。
* 日志轮转：Qt 没有内置 API，自行根据文件大小或时间做简单轮转即可。

配合托盘菜单的两个勾选项（“调试日志(&D)” + “写入日志文件(&L)”），可以在运行时方便控制日志级别和是否落盘，满足开发调试和日常使用的需要。

```
::contentReference[oaicite:27]{index=27}
```


# 11. 配置与数据存储结构

本章统一定义本项目的 **配置文件位置、格式、字段结构**，以及与前面章节中 `AppState` / `DocumentState` / 日志系统 / 自动启动 等模块的关系。

目标：

- 跨平台、路径规范（Qt 官方推荐用 `QStandardPaths` 查找用户配置目录，而不是硬编码路径）。  
- 配置文件是 **人类可读的单个 JSON 文件**，方便备份和手工编辑（不再用 QSettings 存主要配置）。  
- 写入时使用 `QSaveFile` 保证写入失败不会损坏旧配置（官方建议：保存整个文档时使用 QSaveFile）。  

---

## 11.1 配置 / 数据的分类

本项目需要持久化的数据可以分成几类：

1. **应用级设置（Preferences）**
   - 主题名称：`themeName`（如 `"theme-dark"`）
   - 内容透明度：`contentOpacity`（0.0–1.0）
   - 窗口是否始终置顶：`alwaysOnTop`
   - 开机自启（Windows）：`autoStartEnabled`
   - 调试日志开关：`debugLoggingEnabled`
   - 写入日志文件开关：`fileLoggingEnabled`
   - 窗口几何信息：位置、尺寸、最大化状态等

2. **文档级状态（Per-File State）**
   - 每个 `.md` 文件最近阅读位置：`lastScrollY`
   - 最近一次打开时间：`lastOpened`（ISO 8601 字符串）
   - 将来可扩展字段：`customZoom`、`lastSectionId` 等

3. **最近文档列表（Recent Files）**
   - 有序数组：`recentFiles`，存放最近打开的若干路径（例如最多 20 条）

4. **日志 / 缓存（辅助）**
   - 日志文件：单独放在 `logs/reader.log`（第 10 章已定义）:contentReference[oaicite:3]{index=3}  
   - 未来如需缓存（例如解析结果）可在配置目录下新建 `cache/` 目录，不混入主配置 JSON。

---

## 11.2 配置文件位置与命名

### 11.2.1 安装版（推荐）

安装版使用 Qt 官方推荐的 **AppConfigLocation** 作为配置目录：  

- 使用 `QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)`  
- 通常路径类似（按平台有所不同）：
  - Windows：`C:\Users\<User>\AppData\Local\<OrgName>\<AppName>\`  
  - macOS：`~/Library/Preferences/<OrgName>/<AppName>/` 或相近路径  
  - Linux：`~/.config/<OrgName>/<AppName>/`

本项目约定：

- 入口函数中设置应用组织和名称（影响 QStandardPaths 的返回值）：  

```cpp
QCoreApplication::setOrganizationName("zhiz");
QCoreApplication::setApplicationName("TransparentMdReader");
````

* 主配置文件命名为：

```text
<configDir>/transparent_md_reader_state.json
```

其中 `<configDir>` 即 `QStandardPaths::writableLocation(AppConfigLocation)`。

### 11.2.2 便携版（Portable）

为方便你放在 U 盘里带着走，本项目支持一个简单的“便携模式”：

* 如果 exe 同目录下存在空文件（或任意内容文件）：

```text
portable.flag
```

则认为当前为 **便携模式**：

* 配置文件位置改为 exe 同目录：

```text
<exeDir>/transparent_md_reader_state.json
<exeDir>/logs/reader.log
```

* `recentFiles` 中的路径建议存储为相对于 exe 的相对路径，方便整个目录拷贝到另一台机器还能保持历史记录（见 11.5）。

### 11.2.3 `detectConfigPath()` 统一入口

前面章节已经假设存在一个 `detectConfigPath()` 函数，本章给出它的完整定义：

```cpp
QString detectConfigDir()
{
    // portable.flag 在 exe 目录 → 走便携模式
    const QString exeDir = QCoreApplication::applicationDirPath();
    QFileInfo portableFlag(QDir(exeDir).filePath("portable.flag"));
    if (portableFlag.exists()) {
        return exeDir; // 便携版：配置放 exe 目录
    }

    // 安装版：使用 AppConfigLocation
    const QString base = QStandardPaths::writableLocation(
        QStandardPaths::AppConfigLocation);
    QDir dir(base);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    return dir.absolutePath();
}

QString detectConfigPath()
{
    QDir dir(detectConfigDir());
    return dir.filePath("transparent_md_reader_state.json");
}
```

后续所有需要配置路径的模块（状态读写、日志目录路径等）都应通过 `detectConfigPath()` / `detectConfigDir()` 推导，避免散落硬编码。

---

## 11.3 配置文件格式：单一 JSON 文档

### 11.3.1 选择 JSON 的理由

* 文本格式，**易于 diff / 备份 / 手工编辑**。
* Qt 官方自带 JSON 模块（`QJsonDocument / QJsonObject / QJsonArray`），并提供示例，例如 *JSON Save Game* 示例展示了完整的读写流程。
* 社区实践中也普遍把 JSON 用于配置和 profile 这类场景。

**注意**：我们仍然会用 `QSettings` 操作系统级集成（例如 Windows 的 `Run` 注册表键），但那部分只存储“是否开机自启”这一项，主配置全部集中在 JSON 文件中。

### 11.3.2 顶层结构概览（示意）

```jsonc
{
  "version": 1,

  "window": {
    "geometry": "base64-of-QByteArray",
    "onTop": true,
    "lastLocked": true
  },

  "theme": {
    "name": "theme-dark",
    "contentOpacity": 0.85
  },

  "behavior": {
    "autoStartEnabled": true,
    "debugLoggingEnabled": false,
    "fileLoggingEnabled": true
  },

  "recentFiles": [
    "C:/Users/.../docs/a.md",
    "C:/Users/.../docs/b.md"
  ],

  "lastOpenedFile": "C:/Users/.../docs/a.md",

  "documents": {
    "C:/Users/.../docs/a.md": {
      "lastScrollY": 1234,
      "lastOpened": "2025-11-16T12:34:56Z"
    },
    "C:/Users/.../docs/b.md": {
      "lastScrollY": 567,
      "lastOpened": "2025-11-15T22:01:03Z"
    }
  }
}
```

> 说明：
>
> * `version` 用于将来升级配置结构（向后兼容）。
> * `window.geometry` 存放 `QByteArray` 的 base64（Qt 标准做法，和 `saveGeometry()` / `restoreGeometry()` 一致）。
> * `documents` 使用**文件绝对路径字符串**作为 key。便携模式下可以改为以相对路径为 key（见 11.5）。

---

## 11.4 AppState / DocumentState 映射

对应前面章节中使用的状态结构，可以整理成：

```cpp
struct DocumentState {
    qint64   lastScrollY = 0;
    QDateTime lastOpened;
    // 将来可扩展：double zoom; QString lastAnchorId; ...
};

struct AppState {
    int      version = 1;

    // 窗口设置
    QByteArray windowGeometry;
    bool     alwaysOnTop = true;
    bool     lastLocked  = true;   // 上次退出时是否锁定

    // 主题
    QString  themeName = "theme-dark";
    double   contentOpacity = 0.85;

    // 行为
    bool     autoStartEnabled   = false;
    bool     debugLoggingEnabled = false;
    bool     fileLoggingEnabled  = false;

    // 文档相关
    QStringList         recentFiles;
    QString             lastOpenedFile;
    QHash<QString, DocumentState> perFile;  // key = 规范化路径
};
```

`AppState` 与 JSON 的映射：在 `loadAppState()` / `saveAppState()` 中用 `QJsonObject` / `QJsonArray` 手动序列化，类似 Qt 的 JSON 示例：

---

## 11.5 路径规范化与便携模式处理

### 11.5.1 统一路径规范

为了让 `documents` 和 `recentFiles` 的 key 稳定可靠：

1. 加载/保存前统一调用：

```cpp
QString normalizePath(const QString &path)
{
    QFileInfo fi(path);
    return QDir::cleanPath(fi.absoluteFilePath());
}
```

2. 所有对文档路径的操作（加入 `recentFiles`、保存 `perFile` 状态）都先调用 `normalizePath()`。

这样避免同一个文件用不同形式（相对路径 / 不同大小写 / 包含 `.` `..`）导致重复条目。

### 11.5.2 便携模式：相对路径存储

在便携模式下，推荐把 `recentFiles` 和 `documents` 的 key 存成“相对 exe 目录”的路径，便于整个文件夹搬家：

```cpp
QString toStoredPath(const QString &absPath)
{
    if (!isPortableMode()) // 检查 portable.flag
        return normalizePath(absPath);

    QDir base(QCoreApplication::applicationDirPath());
    return base.relativeFilePath(normalizePath(absPath));
}

QString fromStoredPath(const QString &stored)
{
    if (!isPortableMode())
        return normalizePath(stored);

    QDir base(QCoreApplication::applicationDirPath());
    return normalizePath(base.filePath(stored));
}
```

* `saveAppState()`：写 JSON 前，把所有路径都转换成 `toStoredPath()`。
* `loadAppState()`：读 JSON 后，立即用 `fromStoredPath()` 转成当前机器上的绝对路径，再存入 `AppState`。

这样：

* 安装版：直接在本机使用绝对路径即可。
* 便携版：打包整个目录到另一台机器后，路径仍然能正确解析。

---

## 11.6 读取配置：loadAppState()

### 11.6.1 读取流程

> 调用时机：程序启动时，在主窗口创建和文档打开之前。

典型流程：

```cpp
bool MainWindow::loadAppState()
{
    const QString path = detectConfigPath();
    QFile file(path);
    if (!file.exists()) {
        // 第一次启动：使用默认配置
        m_state = AppState{};
        return true;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        // 打不开就使用默认配置，必要时可以弹一次性警告
        m_state = AppState{};
        return false;
    }

    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        // JSON 损坏：可以备份损坏文件，然后重新开始
        m_state = AppState{};
        return false;
    }

    const QJsonObject root = doc.object();

    // 1. 版本
    m_state.version = root.value("version").toInt(1);

    // 2. window
    if (root.contains("window")) {
        const QJsonObject win = root.value("window").toObject();
        const QString geomBase64 = win.value("geometry").toString();
        m_state.windowGeometry = QByteArray::fromBase64(geomBase64.toUtf8());
        m_state.alwaysOnTop = win.value("onTop").toBool(true);
        m_state.lastLocked  = win.value("lastLocked").toBool(true);
    }

    // 3. theme
    if (root.contains("theme")) {
        const QJsonObject theme = root.value("theme").toObject();
        m_state.themeName = theme.value("name").toString("theme-dark");
        m_state.contentOpacity = theme.value("contentOpacity").toDouble(0.85);
    }

    // 4. behavior
    if (root.contains("behavior")) {
        const QJsonObject b = root.value("behavior").toObject();
        m_state.autoStartEnabled    = b.value("autoStartEnabled").toBool(false);
        m_state.debugLoggingEnabled = b.value("debugLoggingEnabled").toBool(false);
        m_state.fileLoggingEnabled  = b.value("fileLoggingEnabled").toBool(false);
    }

    // 5. recentFiles
    m_state.recentFiles.clear();
    const QJsonArray recentArr = root.value("recentFiles").toArray();
    for (const QJsonValue &v : recentArr) {
        const QString stored = v.toString();
        if (stored.isEmpty())
            continue;
        const QString absPath = fromStoredPath(stored);
        if (!absPath.isEmpty())
            m_state.recentFiles << absPath;
    }

    // 6. lastOpenedFile
    const QString lastStored = root.value("lastOpenedFile").toString();
    if (!lastStored.isEmpty())
        m_state.lastOpenedFile = fromStoredPath(lastStored);

    // 7. documents
    m_state.perFile.clear();
    const QJsonObject docs = root.value("documents").toObject();
    for (auto it = docs.begin(); it != docs.end(); ++it) {
        const QString storedKey = it.key();
        const QString absKey    = fromStoredPath(storedKey);
        const QJsonObject docObj = it.value().toObject();

        DocumentState ds;
        ds.lastScrollY = static_cast<qint64>(docObj.value("lastScrollY").toDouble(0));
        const QString ts = docObj.value("lastOpened").toString();
        ds.lastOpened = QDateTime::fromString(ts, Qt::ISODate);

        if (!absKey.isEmpty())
            m_state.perFile.insert(absKey, ds);
    }

    return true;
}
```

读取逻辑与 Qt JSON 示例（Save Game Example）的模式一致：先解析 JSON，再逐字段填充 C++ 结构。

---

## 11.7 写入配置：saveAppState()（QSaveFile）

### 11.7.1 为什么用 QSaveFile

Qt 官方文档明确建议：当写入 **完整文件** 时，应使用 `QSaveFile`，因为它会先写到临时文件，成功后再原子性替换目标文件，避免出现“写了一半就断电，配置文件损坏”的情况。

### 11.7.2 写入流程

> 调用时机：
>
> * 正常退出前（例如 `onActionQuit()` 中）
> * 或在某些关键设置变更后（也可以延迟到退出）

```cpp
bool MainWindow::saveAppState() const
{
    const QString path = detectConfigPath();
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QJsonObject root;
    root.insert("version", m_state.version);

    // 1. window
    {
        QJsonObject win;
        win.insert("geometry", QString::fromUtf8(m_state.windowGeometry.toBase64()));
        win.insert("onTop", m_state.alwaysOnTop);
        win.insert("lastLocked", m_state.lastLocked);
        root.insert("window", win);
    }

    // 2. theme
    {
        QJsonObject theme;
        theme.insert("name", m_state.themeName);
        theme.insert("contentOpacity", m_state.contentOpacity);
        root.insert("theme", theme);
    }

    // 3. behavior
    {
        QJsonObject b;
        b.insert("autoStartEnabled",    m_state.autoStartEnabled);
        b.insert("debugLoggingEnabled", m_state.debugLoggingEnabled);
        b.insert("fileLoggingEnabled",  m_state.fileLoggingEnabled);
        root.insert("behavior", b);
    }

    // 4. recentFiles
    {
        QJsonArray arr;
        for (const QString &absPath : m_state.recentFiles) {
            arr.append(toStoredPath(absPath));
        }
        root.insert("recentFiles", arr);
    }

    // 5. lastOpenedFile
    if (!m_state.lastOpenedFile.isEmpty()) {
        root.insert("lastOpenedFile", toStoredPath(m_state.lastOpenedFile));
    }

    // 6. documents
    {
        QJsonObject docs;
        for (auto it = m_state.perFile.constBegin();
             it != m_state.perFile.constEnd(); ++it) {
            const QString storedKey = toStoredPath(it.key());
            const DocumentState &ds = it.value();

            QJsonObject docObj;
            docObj.insert("lastScrollY", static_cast<double>(ds.lastScrollY));
            if (ds.lastOpened.isValid()) {
                docObj.insert("lastOpened",
                              ds.lastOpened.toString(Qt::ISODate));
            }
            docs.insert(storedKey, docObj);
        }
        root.insert("documents", docs);
    }

    const QJsonDocument doc(root);
    file.write(doc.toJson(QJsonDocument::Indented)); // 方便人工阅读
    return file.commit();
}
```

* 使用 `QSaveFile::commit()` 完成原子替换。
* 使用 `Indented` 格式输出，方便你直接用编辑器打开修改。

---

## 11.8 配置版本与向后兼容策略

随着项目演进，可能会新增字段或调整结构。本章已经预留了：

* 顶层 `version` 字段
* 默认值逻辑：读取配置时，每个字段都带有合理默认值（不存在就用默认）

版本策略建议：

1. **新增字段**：

   * 读取时用 `toBool(default)` / `toDouble(default)` / `toString(default)`
   * 写入时总是写全当前版本定义的字段
     → 这样旧配置也能被新版本平滑读取。

2. **结构调整（例如改 key 或嵌套层级）**：

   * 在 `loadAppState()` 中根据 `version` 做一次迁移：

     * 从旧位置读出值，写入新结构
     * 把 `version` 更新为当前版本
   * 写回时只写入新的结构（旧字段不再写）。

3. **保底策略**：

   * 若 JSON 严重损坏解析失败，可以备份原文件（例如重命名为 `transparent_md_reader_state.json.bak`），再使用默认配置重新开始。

Qt 官方 JSON 示例中也采用了类似“读旧数据 → 填充结构 → 写新数据”的模式，适合在后续版本中演进配置结构。

---

## 11.9 小结

* 配置与状态集中在一个 **人类可读的 JSON 文件** 中，路径由 `QStandardPaths::AppConfigLocation` 或便携模式的 exe 目录统一管理。
* 读写 JSON 全面依赖 Qt 自带的 `QJsonDocument/QJsonObject/QJsonArray` 模块和官方示例模式。
* 保存时用 `QSaveFile` 保证配置写入的原子性，防止中途失败导致配置文件损坏。
* 通过 `version` 字段 + 默认值逻辑 + 可选迁移代码，支持后续平滑扩展配置结构。

本章定义的结构和函数（`detectConfigPath()`、`loadAppState()`、`saveAppState()`、`toStoredPath()` / `fromStoredPath()` 等）是前面第 8 章“文档状态持久化”、第 9 章“单实例”、第 10 章“托盘 + 日志系统”的共同基础。

```

你把上面整块复制进文档就行。如果后面你想再加比如“多配置 profile（不同主题组合）”，我们可以在这一章上再开 11.10 来扩展。
```


# 12. 性能优化

本章的目标是：在支持透明阅读、锁定窗口、链接点击等功能的前提下，让阅读器在以下场景下表现稳定：

- **启动尽量快**：首次打开 md 时延迟可接受，后续打开更快  
- **常驻托盘时 CPU 占用低**：空闲时基本不占资源  
- **大文档滚动流畅**：几十 KB～几 MB 的 md 文件能顺畅阅读  
- **内存占用可控**：长时间开着不会明显涨内存  

围绕这几点，性能优化分为三层：

1. WebEngine/HTML 层  
2. Qt 主线程 / 事件循环层  
3. 构建与部署层（Release、日志等）

---

## 12.1 WebEngine 设置层优化

本项目使用 `QWebEngineView` 渲染 HTML。Qt 官方建议通过 `QWebEngineSettings` 精确控制启用哪些浏览器特性，避免开启用不到的功能，从而减少 CPU / 内存消耗。  

### 12.1.1 只开必要的 WebAttribute

结合官方 `QWebEngineSettings::WebAttribute` 枚举，推荐默认设置：

```cpp
auto *s = webView->settings();

// 保持：需要的功能
s->setAttribute(QWebEngineSettings::AutoLoadImages, true);          // 显示 md 内图片链接时用
s->setAttribute(QWebEngineSettings::JavascriptEnabled, true);       // 用于我们的前端脚本（marked.js 等）
s->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, true); // 本地 html 引用本地资源

// 禁用：当前用不到的功能，减轻负担
s->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, false);
s->setAttribute(QWebEngineSettings::PluginsEnabled, false);         // 不需要 Pepper 插件
s->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, false);
s->setAttribute(QWebEngineSettings::ScreenCaptureEnabled, false);
s->setAttribute(QWebEngineSettings::HyperlinkAuditingEnabled, false);
s->setAttribute(QWebEngineSettings::TouchIconsEnabled, false);

// 按需：硬件加速相关（根据设备情况调试）
s->setAttribute(QWebEngineSettings::WebGLEnabled, false);           // 纯文本阅读一般不需要
s->setAttribute(QWebEngineSettings::Accelerated2dCanvasEnabled, false);
````

说明：

* 上面这些属性名来自 Qt 官方文档中 `WebAttribute` 枚举。
* 关闭 WebGL、2D canvas 加速等，对纯文本 / 简单图片阅读影响不大，但可以减少 GPU 管线和一些后台检查。

### 12.1.2 明确关闭滚动动画

`QWebEngineSettings::ScrollAnimatorEnabled` 控制浏览器端的滚动动画。官方说明是“启用平滑滚动动画”的属性，默认值实现上依 Qt 版本。

考虑到：

* 阅读器自身已经有透明 + 锁定等额外处理
* 多一层平滑动画有时会带来额外 layout/repaint 压力

建议显式关闭：

```cpp
s->setAttribute(QWebEngineSettings::ScrollAnimatorEnabled, false);
```

同时在 CSS 里保证：

```css
html {
  scroll-behavior: auto;
}
```

---

## 12.2 HTML / CSS / JS 结构层优化

虽然只是 md 渲染，但前端结构设计不当也会导致 WebEngine 进程占用偏高。Web 性能实践里，滚动、动画和复杂 CSS 都是常见瓶颈。

### 12.2.1 HTML 结构

建议：

* 每个 md 文件渲染成一个 **单页文档**：`<body><article>...所有内容...</article></body>`
* 标题/段落只用基本标签：`h1–h3`、`p`、`ul/ol/li`、`pre/code`，避免下拉菜单、嵌套 iframe、复杂表格等过度结构。
* 所有交互（滚动、跳转）走浏览器 **原生行为**，不要额外写 js 滚动动画。

### 12.2.2 CSS 规则

* 尽量避免：

  * `box-shadow` 叠加层数太多
  * `filter: blur()`、`backdrop-filter` 这类高开销效果
  * 大量 `position: fixed` / `sticky` 元素
* 透明效果用最简单的方式：

  * 背景透明：`background-color: rgba(0,0,0,0)`
  * 内容轻微透明：`color: rgba(255,255,255,0.9)` 或 body 上设置一个不透明度变量，统一控制

> 对于滚动驱动的动画，业界普遍建议要么关闭，要么非常克制使用，因为滚动过程中浏览器需要不断做布局和绘制。

### 12.2.3 JS 逻辑

* md → HTML 的转换放在 C++ 侧完成：**不在 WebEngine 里跑 heavy 的 Markdown 解析**，WebEngine 只负责展示和简单交互。
* JS 只负责：

  * 初始化渲染好的 HTML（例如设置内部链接点击回调）
  * 控制滚动到指定位置（恢复阅读位置）
* 避免在 `scroll` 事件里做频繁 DOM 操作；若确实需要（例如实时显示当前位置），使用节流（`requestAnimationFrame` 或简单的计数）保证每帧逻辑量有限。

---

## 12.3 Qt 主线程与事件循环

Qt 官方在多处强调：UI 线程应保持事件循环畅通，不要做阻塞操作；计时器数量和事件处理函数的耗时直接影响体验。

### 12.3.1 不在 UI 线程做重 IO / 重计算

* 打开 md 文件：

  * 一般大小（几十 KB）可以直接在 UI 线程用 `QFile` 读；
  * 对于异常大的 md（> 1–2MB），可以考虑：

    * 用 `QThread` / 线程池后台读取与解析
    * 完成后通过 `signal/slot` 把 HTML 交回主线程加载
* 禁止在 UI 线程里使用 `sleep()` 阻塞事件循环；Qt 官方和社区经验都建议用 `QTimer` + 状态机代替。

### 12.3.2 计时器与定时任务

性能实践中，“计时器太多”是典型问题之一：事件循环挤满唤醒，CPU 空转。

针对本项目：

* 能不用计时器的地方尽量不用：

  * 阅读位置更新可以只在滚动停止时、或每次用户切换文档时保存
* 若需要周期任务（例如定时 flush 配置）：

  * 统一一个 `QTimer` 做“心跳”，集中检查一些标志位，而不是各自一个 timer

### 12.3.3 窗口锁定 / 透明时的刷新

透明窗口本身只要没有反复 `update()`，Qt 不会无意义重绘。一般建议：

* 只在内容变化（打开新 md / 主题切换）时触发必要的重绘；
* 锁定 / 解锁仅改变 window flag 与输入穿透状态，不在锁定逻辑里频繁重绘；
* 不在 `mouseMoveEvent` 里做大量操作，拖动窗口时只调整位置。

---

## 12.4 大文档与滚动体验

QWebEngine 底层使用 Chromium，天生能处理长页面，但结合透明、置顶等特性时，仍然需要注意几件事。

### 12.4.1 避免反复重新加载整页

* **原则：一次 md 打开 → 一次 HTML 生成 → 一次 `setHtml()`**
* 切换章节或内部锚点时只做 `window.scrollTo()` 或 `location.hash` 跳转，不重新 setHtml
* 文件修改检测：

  * 若支持“外部修改自动刷新”，通过 `QFileSystemWatcher` 检测到变更再整体重新加载；
  * 平时不要因为一些小设置（比如透明度）就重新渲染 HTML。

### 12.4.2 滚动逻辑

* 右键上下半区翻页、滚轮滚动等，全部委托给浏览器滚动；不要额外再叠加第二层动画。
* Ctrl 临时解锁时，滚轮直接作用于 WebEngine，结束后恢复锁定即可，不需要在 C++ 层做插值滚动。

---

## 12.5 启动时间与常驻资源

Qt 社区里对 QtWebEngine 的常见反馈是：“**首次启动较慢**，并且会常驻一个或多个 `QtWebEngineProcess` 子进程占用内存和 CPU”。

### 12.5.1 WebEngineProfile 复用

官方说明中，`QWebEngineProfile` 提供共享缓存、cookie 等能力，也建议在应用内尽量复用 profile。

本项目建议：

* 创建一个全局 `QWebEngineProfile*`（例如 `AppProfile::instance()`），所有 `QWebEngineView` 共享同一个 profile；
* 只保留一个 `QWebEngineView` 实例，不在运行时反复创建/销毁；
* 托盘常驻时，只是隐藏主窗口，不销毁 view，这样避免下次打开再初始化 Chromium。

### 12.5.2 空闲时 CPU 占用

在一些浏览器类项目（如 qutebrowser）里，开发者通过禁用某些 WebEngine 功能、减少 JS 定时器、限制动画等方式来降低空闲 CPU。

本阅读器因为内容简单，可以做到：

* 前端无无限循环动画、无定时器轮询；
* 禁用不必要的 WebAttribute（见 12.1）；
* 隐藏窗口后不再主动执行任何滚动、渐变之类的操作。

---

## 12.6 构建与日志对性能的影响

### 12.6.1 一定要用 Release 构建

在 Qt 社区和一般 C++ 开发经验中，Debug 版本因为关闭优化、保留大量调试信息，会比 Release 明显慢，二进制也更大。

建议：

* 日常开发调试：Debug
* 自己日常使用 / 真正打包发布：Release
* CMake / qmake 中开启优化选项（常规的 `/O2`、`-O2` 等，按平台默认即可）。

### 12.6.2 日志对性能的影响

第 10 章里已经设计了“调试日志 + 写入文件”两个开关，性能上要注意：

* 写入日志文件时：

  * 高频 `qCDebug` 会带来 IO 负担
  * 所以“调试日志(&D)”建议只在定位问题时临时打开
* 日志轮转策略（按文件大小）已经避免日志无限膨胀，但仍应：

  * 不在渲染热点（滚动、鼠标移动事件）中频繁打 debug 日志
  * 把关键日志集中在文档切换、配置加载/保存、异常路径等少数节点

---

## 12.7 性能分析工具

Qt 官方近两年持续在加强性能工具链，推荐在开发阶段用这些工具定位瓶颈。

### 12.7.1 Qt 自带工具

* Qt Creator 的 **CPU / 内存分析器**：看主线程是否有长时间函数、是否频繁分配内存
* `QElapsedTimer`：在关键路径（打开文件、渲染 md）手动打点统计耗时

### 12.7.2 WebEngine / Chromium DevTools

Qt WebEngine 官方支持通过远程调试端口打开 Chromium DevTools，用来检查 Web 内容的 layout / paint / JS 性能。

典型用法：

* 启动应用时设置环境变量或命令行参数，例如：

  * 环境变量：`QTWEBENGINE_REMOTE_DEBUGGING=9222`
  * 或运行参数：`--remote-debugging-port=9222`
* 用 Chrome / Edge 打开 `http://localhost:9222`，选择对应页面
* 通过 DevTools 的 Performance/Memory/Network 面板检查：

  * 是否有不必要的重绘
  * 是否存在 JS 长任务
  * 是否有网络请求（理论上本地 md 阅读不应发网络）

---

## 12.8 小结

1. **WebEngine 设置层**：用 `QWebEngineSettings` 精简启用的特性，关闭插件、全屏、滚动动画等无关功能。
2. **HTML/CSS/JS 层**：传统 Web 性能经验同样适用——结构简单、少动画、不在滚动事件里做重逻辑。
3. **Qt 主线程层**：遵守“UI 线程不做重活”的原则，合理使用 QTimer、后台线程，保持事件循环顺畅。
4. **构建与日志**：Release 构建 + 可控的日志开关，确保调试方便和运行效率之间平衡。
5. **分析工具**：Qt Creator + Chromium DevTools 可以在真实页面上直接看到布局和脚本的开销，避免“凭感觉”优化。

后续如果你在实际使用中发现某些场景（比如 5MB 以上超级长文档）还有卡顿，可以在这个章节再加一个 12.9“小结案例：大文件优化实践”，专门记录针对这些案例的调整方案。

```
```


# 13. 可扩展性规划

本章不做具体功能实现，主要规划本项目后续“加功能不翻车”的架构思路，重点是：

- 当前只针对 **Windows 平台** 开发；
- 把将来可能会加的功能、平台，提前设计成 **扩展点**；
- 让现有模块（渲染层、状态管理、单实例、托盘、日志等）在结构上便于挂新功能。

---

## 13.1 总体原则

1. **核心逻辑与平台逻辑分离**

   - Windows 专用的部分集中封装，例如：
     - 自动启动（注册表 `Run` 键）；
     - 必要的原生窗口处理（如果未来有）。
   - 其它逻辑尽量用纯 Qt：
     - Markdown 渲染；
     - 文档状态管理（AppState / DocumentState）；
     - 单实例通信（QLocalServer/QLocalSocket）；
     - 托盘（QSystemTrayIcon）；
     - 日志系统（QLoggingCategory + qInstallMessageHandler）。

2. **所有“入口点”都走统一接口**

   入口包括：

   - 托盘“打开文件…”菜单；
   - 拖放 `.md` 到窗口；
   - 拖放 `.md` 到 exe（命令行参数）；
   - 双击 `.md`（文件关联）；
   - 内部链接跳到其它 `.md`（第 7 章）；
   - 单实例收到新文件请求（第 9 章）。

   统一收敛到一个高层函数，例如：

   ```cpp
   void MainWindow::navigateTo(const QString &path);
    ````

    由它负责：

    * `captureScrollPosition()` 保存当前文档滚动位置；
    * 更新 `AppState::recentFiles` / `perFile`；
    * `openMarkdownFile(path)` → 通知 WebEngine 渲染；
    * `restoreScrollPosition(path)` → 恢复阅读位置。

    以后再新增入口（比如“搜索结果点击”、“注释列表跳转”），都只需要调用 `navigateTo()`，减少逻辑分叉。

3. **“功能模块化 + 配置驱动”**

   * 主题系统（第 6 章）：新增主题只需要多几套 CSS / 预设值，不动核心逻辑；
   * 日志系统（第 10 章）：调试/写文件开关都走 AppState + 托盘菜单，不散落魔法开关；
   * 状态持久化（第 14 章）：所有新功能的“长期状态”都走 AppState → JSON 这一条链。

---

## 13.2 渲染层扩展点

### 13.2.1 多渲染模式

当前推荐设计：

* C++ 侧：

  * 负责读 md；
  * 可选：调用本地 Markdown 库先转 HTML，或者直接把 md 交给前端。
* WebEngine 前端：

  * 负责 HTML 渲染；
  * 主题/透明度控制；
  * 链接点击处理。

扩展方向：

1. **渲染模式枚举**

   ```cpp
   enum class RenderMode {
       Normal,        // 普通 md 阅读
       FocusCode,     // 代码学习增强模式
       MinimalNote    // 极简复习模式
   };
   ```

   * 在 AppState 中增加 `renderMode` 字段；
   * 加托盘或快捷键切换；
   * 前端 JS / CSS 根据 `RenderMode` 切不同 class 或模板。

2. **Markdown 渲染后端可替换**

   抽象一层接口，例如：

   ```cpp
   class IMarkdownRenderer {
   public:
       virtual ~IMarkdownRenderer() = default;
       virtual QString renderToHtml(const QString &markdown) = 0;
   };

   class SimpleMarkdownRenderer : public IMarkdownRenderer {
   public:
       QString renderToHtml(const QString &markdown) override;
   };
   ```

   * 当前可以简单实现（甚至直接不做转换，给前端处理）；
   * 将来要换其它库（更好的表格、数学公式支持），只改实现，不动上层。

---

## 13.3 状态管理扩展点（AppState / DocumentState）

`AppState` / `DocumentState` 已在第 14 章定义，这里从扩展角度再看一眼：

```cpp
struct DocumentState {
    qint64    lastScrollY = 0;
    QDateTime lastOpened;
    // TODO: double zoom;
    // TODO: QString lastHeadingId;
    // TODO: QVector<Annotation> annotations;
};

struct AppState {
    int       version = 1;

    QByteArray windowGeometry;
    bool       alwaysOnTop = true;
    bool       lastLocked  = true;

    QString    themeName = "theme-dark";
    double     contentOpacity = 0.85;

    bool       autoStartEnabled    = false;
    bool       debugLoggingEnabled = false;
    bool       fileLoggingEnabled  = false;

    QStringList          recentFiles;
    QString              lastOpenedFile;
    QHash<QString, DocumentState> perFile;
};
```

以后扩展：

* 文档级：

  * 每文档记住缩放：`double zoom`；
  * 记住上次所在标题 id：`QString lastHeadingId`；
  * 注释列表：`QVector<Annotation>`；
* 应用级：

  * 自定义快捷键配置；
  * 搜索历史（比如全局搜索的最近关键字）。

都可以直接往这两个 struct 里加字段，再在 `loadAppState()` / `saveAppState()` 里补 JSON 映射。

---

## 13.4 搜索功能扩展点

### 13.4.1 当前文档内搜索

目标：

* 在当前 md 文档内搜索关键字；
* 滚动到第一个匹配位置，并高亮所有匹配。

扩展设计：

* UI：

  * 托盘菜单加“搜索(&F)…”；
  * 或顶部加一个隐藏的“搜索条”（只在未锁定或 Ctrl 解锁时显示）。
* 逻辑：

  * C++：弹出输入框拿到关键字，调用前端 JS；
  * JS：在 DOM 中搜索，给匹配节点加高亮 class，并滚动到第一个匹配位置。

对 AppState 的影响：

* 如需保存“上次搜索关键字”，可在 AppState 中加一个 `lastSearchText` 字段；
* 否则可以不持久化，只在一次会话中使用。

### 13.4.2 多文档搜索（可选）

后续扩展方向：

* 维护一个简单内存索引：

  * 针对 `recentFiles` 列表中的若干 md 做扫描；
  * 支持按关键字搜索文件列表。
* 搜索结果列表：

  * 单独一个 Qt 对话框列出 “文件名 + 摘要”；
  * 点击结果调用 `navigateTo(path)` 打开文档；
  * 进入文档后再由当前文档搜索逻辑跳到第一个匹配位置。

---

## 13.5 注释 / 高亮扩展点

分阶段考虑：

1. **临时高亮（不持久化）**

   * 在前端允许用户选中一段文本，点击“高亮”按钮；
   * JS 给对应元素加一个高亮 class；
   * 不写入 AppState，关闭程序后消失。

2. **持久化注释**

   * `DocumentState` 增加：

     ```cpp
     struct Annotation {
         QString anchorId;   // 可用 heading id / 段落索引 / 文本 hash
         QString note;       // 注释内容
         QDateTime created;
     };
     QVector<Annotation> annotations;
     ```

   * 页面加载完毕后，把注释列表传给 JS，让它根据 anchorId 定位元素，加高亮/小标记；

   * 在前端新增/编辑/删除注释时，通过 JS → C++ 回调更新 `DocumentState` 并 `saveAppState()`。

3. **注释导航**

   * 托盘菜单增加“打开注释面板”；
   * 弹一个 Qt 窗口列出当前文档的注释（时间、摘要）；
   * 点击条目，调用 `navigateTo(path)` 并让前端滚动到对应 anchor。

---

## 13.6 轻量级“功能模块”机制

先不做真正的插件系统（DLL / .so），但可以预留一个“模块接口”，避免所有扩展逻辑都直接写进 MainWindow：

```cpp
class FeatureModule {
public:
    virtual ~FeatureModule() = default;
    virtual void onAppStarted(MainWindow *w) {}
    virtual void onDocumentOpened(const QString &path) {}
    virtual void onBeforeQuit() {}
};
```

* 在 `MainWindow` 中维护：`QVector<std::unique_ptr<FeatureModule>> m_modules;`
* 启动后按需构造模块，比如：

  * `m_modules.push_back(std::make_unique<LoggingFeatureModule>());`
  * 以后再加 `AnnotationFeatureModule`、`SearchFeatureModule` 等；
* 打开文档 / 退出前分别调各个模块对应回调。

将来如果真的想做 **外部插件**：

* 可以用 `QPluginLoader` 从某个 `plugins/` 目录动态加载；
* 要求插件导出一个工厂函数，返回继承自 `FeatureModule` 的对象；
* 不会破坏当前架构，只是在现有机制上多了一层“从 DLL 创建模块”的步骤。

---

## 13.7 配置与扩展功能的关系

扩展功能与配置的关系原则：

1. **所有“会影响下次启动”的状态都走 AppState / JSON**

   * 例如：

     * 当前渲染模式；
     * 搜索 / 注释相关的选项；
     * 自定义快捷键；
     * 某些模块是否启用。
   * 避免零散写各种 ini / 自己文件。

2. **给扩展功能预留 version**

   * 顶层已经有 `AppState::version`；
   * 如某个子系统（比如注释系统）结构大改，可以给它再加一个子 version，分阶段迁移。

3. **读写逻辑要有“默认值”**

   * 新字段只在新版本写入，旧版本读不到也能正常运行；
   * 读取时统一有 fallback（`toBool(default)` / `toInt(default)` / `toString(default)`）。

---

## 13.8 跨平台扩展（作为后续扩展点）

当前明确目标：**只支持 Windows**，并且可以充分利用 Windows 特性（注册表开机自启、任务栏行为等）。

但未来如果要扩展到 macOS / Linux，为了降低重写成本，可以现在就做几件准备工作：

1. **平台相关代码集中封装**

   已经按前面章节做了这几类封装：

   * 自动启动：封装成 `AutoStartHelper` 类：

     * 目前内部用 Windows 注册表；
     * 将来在 macOS 可以实现为 Login Items，Linux 可以用 `~/.config/autostart`；
     * 上层代码始终只调用 `AutoStartHelper::isSupported()/isEnabled()/setEnabled()`。
   * 打开外部程序 / 链接：

     * 一律用 `QDesktopServices::openUrl()`，不直接调用 `ShellExecute` 等 WinAPI。

2. **不在核心逻辑里到处写 `#ifdef Q_OS_WIN`**

   * 必须用到时，优先在 helper 里写 `#ifdef`；
   * `MainWindow` / `AppState` 等核心类里尽量避免平台 ifdef；
   * 这样将来把 helper 实现替换为 macOS / Linux 版本时，业务层不用改。

3. **路径 / 配置位置走 Qt 标准接口**

   * 已经通过 `QStandardPaths::AppConfigLocation` + `portable.flag` 统一配置路径；
   * 在非 Windows 平台上，只要 Qt 自己返回对应系统的推荐位置即可；
   * 便携模式本身就是跨平台的（exe 目录 / AppImage / bundle 内相对路径）。

4. **跨平台作为一个“扩展方向”，而不是当前约束**

   * 现在所有设计都以 Windows 为第一优先；
   * 但在本文档中明确记下“跨平台要改哪些点”（主要集中在 AutoStart / 文件关联 / 安装与打包侧）；
   * 以后如果你真的要上 macOS / Linux，可以单独开一个章节（比如 `# 18. 跨平台实现计划`），渐进式地实现，而不用重写整个应用。

---

## 13.9 小结

* 架构层面已经把 **入口统一、状态集中、平台封装** 这几件事提前做好，为未来扩展留出空间；
* 功能层面的典型扩展点包括：

  * 多渲染模式；
  * 文档内 / 多文档搜索；
  * 注释 / 高亮；
  * 轻量级“功能模块”甚至真正插件；
* “跨平台”不作为当前版本的硬要求，而是列在扩展清单里：

  * 通过 helper + Qt 标准接口，把平台差异点集中起来；
  * 保证以后要扩平台时，工作量主要集中在少量模块，而不是大规模重构。

```

```


# 14. 工程目录结构规范

本章对第 2 章的架构做工程目录层面的落地。
本章约定 **仓库目录结构与命名约定**，目标是：

1. 新增/重构模块时有明确落点，不用临时随便建目录；
2. Qt / C++ 圈子里看到这个结构的人，一眼就能理解；  
3. 和前文的“配置与数据存储结构”“单实例机制”“系统托盘设计”等章节自然对齐。

> 注意：本章只约定 **工程仓库内的目录**，运行时配置文件 / 日志 / 缓存等的物理位置，仍以“配置与数据存储结构”章节为准。

---

## 14.1 顶层目录布局

推荐整体结构（仓库根目录）：

```text
TransparentMdReader/
├── CMakeLists.txt          # 顶层 CMake 配置（Qt 官方推荐用 CMake）  
├── README.md               # 项目说明  
├── LICENSE                 # 许可证（可选）  
├── src/                    # 所有 C++ 源码（主工程）  
├── resources/              # HTML/CSS/JS/图标等静态资源（由 .qrc 收集）  
├── tests/                  # 单元测试 / 集成测试  
├── tools/                  # 脚本、小工具（打包、生成代码等）  
├── docs/                   # 设计文档、开发文档（本 md 可以放这里）  
├── cmake/                  # CMake 辅助模块（可选）  
└── build*/                 # 构建输出目录（忽略入版本控制）  
````

* `build*/` 建议本地使用 `build-debug/`、`build-release/` 一类目录，放在仓库根目录之外或通过 `.gitignore` 排除，这和通用 C/C++ 工程实践一致。

---

## 14.2 src/ 目录细分

`src/` 下按“职责”分模块，避免所有文件堆在一个目录里：

```text
src/
├── app/                # 应用入口 & 顶层窗口
│   ├── main.cpp
│   ├── MainWindow.h / .cpp
│   ├── TrayManager.h / .cpp         # 系统托盘相关
│   └── AppController.h / .cpp       # 可选：集中管理导航、状态等
│
├── core/               # 与 UI 无关的核心逻辑
│   ├── DocumentState.h / .cpp       # 文档状态持久化（第 8 章）
│   ├── AppState.h / .cpp            # 全局状态（配置、最近文档等）
│   ├── Settings.h / .cpp            # 读写 JSON 配置
│   └── Logger.h / .cpp              # 日志（第 10 章）
│
├── render/             # 渲染 / WebEngine 相关
│   ├── WebViewHost.h / .cpp         # 封装 QWebEngineView
│   ├── MarkdownPage.h / .cpp        # 自定义 QWebEnginePage（链接拦截）
│   └── Backend.h / .cpp             # QWebChannel 后端对象
│
├── ui/                 # 纯 UI 相关（布局、小控件）
│   ├── LockButton.h / .cpp          # 🔒 按钮封装（可选）
│   ├── TitleBar.h / .cpp            # 自绘标题栏 / 按钮栏
│   └── resources.qrc                # 图标、SVG 等（也可放在 resources/ 根下）
│
├── platform/           # 平台相关封装（当前主要是 Windows）
│   ├── win/  
│   │   ├── SingleInstance_win.h / .cpp   # QLocalServer + QLocalSocket 封装
│   │   ├── AutoStart_win.h / .cpp       # 开机自启（注册表）
│   │   ├── FileAssoc_win.h / .cpp       # 文件关联 / 拖到 exe
│   │   └── WinApiHelpers.h / .cpp       # SetForegroundWindow 等辅助
│   └── common/
│       └── Paths.h / .cpp               # 配置路径检测（便携版/安装版统一封装）
│
└── main.qrc             # 汇总所有资源（HTML/CSS/JS/图标等）
```

设计要点：

1. **app/** 只做“组装”：窗口、托盘、信号连接，不写业务细节；
2. **core/** 可以单独拿出来写测试，不依赖 UI；
3. **render/** 聚合所有 WebEngine 相关类，便于后续更换渲染方案；
4. **platform/** 把 Windows 特定逻辑装进一个角落，后续做跨平台时只改这里；
5. **ui/** 可以视规模决定要不要拆；当前项目 UI 很轻，可以少量文件就够。

---

## 14.3 resources/ 目录细分（配合 .qrc）

资源目录主要为：

* `index.html` / `markdown.css` / `reader_theme.css` 等前端文件；
* `marked.min.js` 等第三方 JS 库；
* 背景图片、图标等。Qt 官方推荐把这些放在资源系统管理下，通过 `.qrc` 打包到二进制中。

建议结构：

```text
resources/
├── html/
│   ├── index.html           # WebEngine 主页面
│   ├── markdown.css         # Markdown 基础样式
│   └── reader_theme.css     # 本项目的主题/透明度样式（第 6 章）
│
├── js/
│   └── marked.min.js        # 第三方 Markdown 库（保持原始文件名）
│
├── images/
│   ├── icons/               # PNG/SVG 图标
│   └── backgrounds/         # 背景图（纸张纹理、壁纸）
│
└── qml/                     # 如后续引入 QML，可另建子目录（当前可为空）
```

对应在一个或多个 `.qrc` 文件中维护：

```xml
<RCC>
  <qresource prefix="/">
    <file>resources/html/index.html</file>
    <file>resources/html/markdown.css</file>
    <file>resources/html/reader_theme.css</file>
    <file>resources/js/marked.min.js</file>
    <file>resources/images/icons/app.ico</file>
    <file>resources/images/backgrounds/paper_texture.png</file>
  </qresource>
</RCC>
```

> 可以按类别分多个 `.qrc`（如 `ui.qrc` / `web.qrc`），在 `CMakeLists.txt` 里统一加到目标。

---

## 14.4 tests/、tools/、docs/ 约定

这些目录遵循通用 C/C++ 工程实践：

```text
tests/
├── CMakeLists.txt        # 测试目标
├── test_core_documentstate.cpp
├── test_appstate.cpp
└── test_paths.cpp

tools/
├── generate_icons.ps1    # 生成多尺寸图标（可选）
├── pack_portable.cmd     # 打包便携版 zip
└── make_installer.iss    # Inno Setup 脚本（如使用）

docs/
├── TransparentMarkdownReader_Design.md       # 总体设计
└── 透明阅读器开发文档.md                       # 你现在在写的这个文件
```

* 测试推荐用 Qt Test（QTestLib）或 Catch2 / GoogleTest 等；目录结构和主工程尽量平行，便于按模块加测试。
* `tools/` 中的脚本不参与构建，只作为开发辅助；
* 文档统一放入 `docs/`，避免和源码夹在一起。

---

## 14.5 CMake / Qt 项目文件布局

Qt 6 官方以 CMake 为推荐构建系统，这里假设使用 CMake。

典型组织方式：

```text
CMakeLists.txt          # 顶层
src/CMakeLists.txt      # 主目标
tests/CMakeLists.txt    # 测试目标
resources/CMakeLists.txt# 可选：专门处理资源
cmake/                  # 自定义 CMake 模块（如代码生成、版本号）
```

顶层 `CMakeLists.txt` 示例逻辑（伪代码）：

```cmake
# 设定项目名/语言/Qt 版本
project(TransparentMdReader LANGUAGES CXX)

add_subdirectory(src)
add_subdirectory(tests)        # 可选
```

`src/CMakeLists.txt` 中：

* 定义主可执行文件目标 `transparent_md_reader`；
* 链接 `Qt::Widgets`, `Qt::WebEngineWidgets`, `Qt::WebChannel` 等；
* 添加 `.qrc` 资源文件；
* 按模块组织源码（`app/`, `core/`, `render/`, `platform/` 等）。

> 具体 CMake 配置已经属于构建系统细节，不在本章展开，仅约定 **每个目录有自己的 CMakeLists.txt**，并尽量保持“一个目录一个模块”的习惯。

---

## 14.6 文件命名与命名空间约定

为避免项目变大之后混乱，本项目约定：

1. **头/源成对出现**：

   * `ClassName.h` + `ClassName.cpp` 放在同一目录；
   * 公有接口类放在所属模块目录（如 `core/DocumentState.h`）。

2. **命名空间按模块划分**：

   ```cpp
   namespace tmr {             // Transparent Markdown Reader 缩写
   namespace core {            // 核心逻辑
       class DocumentState { ... };
   }
   namespace render {
       class WebViewHost { ... };
   }
   namespace platform {
   namespace win {
       class AutoStartWin { ... };
   }
   }
   }
   ```

3. **平台相关后缀**：

   * Windows 专用实现用 `_win` 后缀：`AutoStart_win.cpp`、`SingleInstance_win.cpp`；
   * 未来如有其他平台，可以追加 `_mac` / `_linux`。

4. **资源命名**：

   * 图标文件使用语义化命名（`icon_lock.png`, `icon_tray_main.ico`），避免 `1.png`, `2.png` 这类名字；
   * CSS/JS 保持原始库名（如 `marked.min.js`）以便后期升级。

---

## 14.7 与其他章节的关系

1. **与“配置与数据存储结构”章节**

   * 本章只定义 *仓库内* 的 `resources/`, `src/`, `docs/` 等目录；
   * 真正的运行时配置路径（便携版/安装版、AppData、日志目录）仍由“配置与数据存储结构”章节负责，那里会基于 `platform/common/Paths` 做统一封装。

2. **与“可扩展性规划”章节**

   * 预留了 `core/`, `render/`, `platform/`, `tests/` 等清晰边界，便于后续增加：

     * 搜索、书签、大纲树等核心功能 → 放在 `core/` + `render/`；
     * 跨平台行为 → 扩展 `platform/` 子目录；
     * 更多工具/打包脚本 → 放在 `tools/`。

3. **与“性能优化 / 单实例 / 拖放 / 托盘”章节**

   * 单实例 / 拖放 / 托盘等模块的代码位置在本章已预留（`platform/win`, `app/TrayManager` 等），避免后续代码散落。

---

## 14.8 小结

* 顶层目录采用 **src + resources + tests + tools + docs + build*** 的常见布局，对齐主流 C++/Qt 项目实践；
* `src/` 内按职责划分为 `app/`, `core/`, `render/`, `ui/`, `platform/`，让窗口逻辑、核心逻辑和平台特性清晰分层。
* `resources/` 收拢 HTML/CSS/JS/图片等静态资源，通过 `.qrc` 打包给 Qt WebEngine 使用。
* 为测试、工具脚本、文档预留固定位置，方便后续扩展而不破坏结构。
* 命名空间和文件命名约定，保证项目规模变大后依然易于维护和导航。

```
```

