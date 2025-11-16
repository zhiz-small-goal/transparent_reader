# AI_README

> 本文件是专门写给“AI 帮我写代码”的说明，人类开发时可以参考，但以主开发文档为准。

## 1. 项目概述

- 名称：TransparentMdReader（Markdown 透明阅读器）
- 平台：Windows 10/11，桌面应用
- 语言：C++17
- 框架：Qt 6.x（Qt Widgets + Qt WebEngine + Qt WebChannel）
- 主要特性：
  - 无边框 + 可拖动 + 置顶 的透明窗口
  - 渲染本地 Markdown 文件为 HTML，使用 WebEngine 显示
  - 支持内部链接 / 外部链接点击跳转
  - 记住每个文档的阅读进度和最近打开列表
  - 单实例：新的打开请求转发给已运行实例
  - 拖拽文件到窗口或 exe 打开
  - 系统托盘图标与菜单（最近文档、退出等）
  - 主题与配置（透明度、字体、主题色等）

## 2. 非目标

在没有额外说明前，请不要实现以下内容：

- 不做 Markdown 编辑器功能（只读）
- 不做云同步 / 网络账户 / 登录
- 不做插件系统 / 脚本引擎
- 不做跨平台适配（只考虑 Windows）
- 不做多窗口和复杂布局系统（一个主窗口 + 一个 Web 视图即可）

## 3. 运行环境与依赖

- C++ 标准：C++17（不要使用协程 / Concepts / Ranges 等 C++20+ 特性）
- Qt 版本：6.x
- 使用 Qt 官方模块：
  - Qt::Core
  - Qt::Gui
  - Qt::Widgets
  - Qt::WebEngineWidgets
  - Qt::WebChannel
- Markdown 解析：
  - 可用任意成熟库（例如 cmark-gfm），如未特别指定，按常见用法写，方便后续替换。

## 4. 工程目录结构约定

工程目录约定如下（AI 实现时请按此结构输出示例）：

```text
project_root/
  CMakeLists.txt
  /src
    CMakeLists.txt
    /app          # 程序入口、MainWindow
    /core         # AppState / DocumentState / AppConfig 等
    /platform     # 单实例、托盘相关封装
    /ui           # 设置对话框、主题相关 UI（如有）
    /web          # WebChannel 桥接类、与前端交互的封装
  /resources
    /web          # index.html, main.js, style.css 等
    /themes       # 主题配置 JSON 或 CSS
    /icons        # 托盘图标等
```

后续如需新增模块，优先放入以上目录之一，不要在根目录堆大量 .cpp。

## 5. 核心类与职责概览

### 5.1 MainWindow（src/app/MainWindow.*）

- 继承：`QMainWindow`
- 成员（示例）：

```cpp
QWebEngineView* m_view;
QWebChannel* m_channel;
AppBridge* m_bridge;
// 指向 AppState、AppConfig 的引用或指针
```

- 主要职责：
  - 初始化 UI（无边框、透明、置顶等）
  - 创建并配置 `QWebEngineView` 与 `QWebChannel`
  - 打开 Markdown 文件：`openMarkdownFile(const QString& path)`
  - 恢复 / 保存当前文档的滚动位置
  - 处理拖放事件（拖入 md 文件打开）
  - 处理来自单实例模块的“打开文件”请求
  - 将托盘 / 菜单动作映射到实际行为（打开文件、退出等）

### 5.2 AppState（src/core/AppState.*）

- 描述应用运行期的整体状态（在内存中）。
- 持有：
  - 当前打开文档的路径
  - 已打开文档的 `DocumentState` 映射（按绝对路径）
  - 最近打开文档列表
- 提供方法示例：

```cpp
DocumentState& getOrCreateDocumentState(const QString& path);
void updateScrollPosition(const QString& path, double ratio);
void touchRecentFile(const QString& path);
```

### 5.3 DocumentState（src/core/DocumentState.* 或放在 AppState 中）

- 字段示例：

```cpp
QString filePath;
QDateTime lastOpenTime;
double scrollRatio;      // 0.0 ~ 1.0
qint64 fileLastModified;
```

- 语义：
  - 记录单个文档的阅读进度和最近访问时间。

### 5.4 AppConfig（src/core/AppConfig.*）

- 管理配置文件读写。
- 使用 JSON 文件存储配置信息。
- 推荐路径：
  - 使用 `QStandardPaths::AppConfigLocation` 组合应用名构造目录。
- 配置内容示例：
  - 窗口大小、位置
  - 是否置顶
  - 透明度
  - 当前主题名
  - 最近打开的文件数量上限等

### 5.5 MarkdownRenderer（src/core/MarkdownRenderer.*）

- 负责将 Markdown 文本转换为 HTML 字符串。
- 对外接口示例：

```cpp
QString renderToHtml(const QString& markdownText);
```

- 行为：
  - 只做纯文本转换，不关心 WebEngine 或状态管理。

### 5.6 AppBridge（src/web/AppBridge.*）

- 继承：`QObject`
- 通过 `QWebChannel` 暴露给前端 JavaScript。
- 示例接口（`Q_INVOKABLE`）：

```cpp
Q_INVOKABLE void onLinkClicked(const QString& href);
Q_INVOKABLE void onScrollChanged(double ratio);
Q_INVOKABLE void requestOpenFile(const QString& path);
```

- 示例信号：

```cpp
void setScrollPosition(double ratio);
void applyTheme(const QString& themeName);
```

- 行为：
  - 将前端事件（链接点击、滚动变化）转发给 C++（`MainWindow` / `AppState`）。
  - 将 C++ 的状态变化（需要设置滚动位置 / 应用主题）通知前端。

### 5.7 SingleInstanceGuard（src/platform/SingleInstanceGuard.*）

- 封装单实例逻辑。
- 内部使用 `QLocalServer` / `QLocalSocket`。
- 主要接口：

```cpp
bool tryRunAsPrimary();
// 返回 true 表示本进程是主实例。

bool sendOpenFileMessage(const QStringList& paths);
// 向已有实例发送“打开这些文件”的消息。

void setMessageHandler(std::function<void(const QStringList&)> handler);
// 主实例用于处理收到的“打开文件”请求。
```

### 5.8 TrayManager（src/platform/TrayManager.*）

- 封装系统托盘图标和菜单。
- 职责：
  - 显示托盘图标
  - 托盘菜单项：显示 / 隐藏窗口、最近文件列表、退出
  - 将菜单动作通过回调或信号转发给外层（例如 `MainWindow`）

## 6. Web 前端约定（resources/web）

### 6.1 文件结构

```text
/resources/web/
  index.html
  main.js
  style.css
```

### 6.2 index.html

- 启动时加载 `main.js` 并设置 `QWebChannel`。
- 提供一个容器元素用于显示渲染后的 HTML（由 C++ 注入）。

### 6.3 main.js

- 在 `QWebChannel` 初始化后，获取 `window.appBridge`。
- 向 C++ 报告：
  - 文档加载完成事件
  - 滚动条变化（节流后调用 `appBridge.onScrollChanged(ratio)`）
  - 点击链接事件（解析内部 / 外部链接，调用 `appBridge.onLinkClicked(href)`）
- 接收 C++ 通知：
  - 设定滚动位置：监听 `setScrollPosition` 信号，对应设置滚动条。
  - 应用主题：根据主题名为 `body` 设置 class，例如 `theme-dark`。

### 6.4 style.css

- 定义主题相关样式（背景色、字体、行间距等）。
- 支持根据 `body` 上的 class（例如 `theme-dark` / `theme-light`）切换主题。

## 7. 存储格式约定

### 7.1 配置文件（config.json）

- 位置：使用 `QStandardPaths::AppConfigLocation`，文件名可为 `config.json`。
- 示例结构：

```json
{
  "window": {
    "width": 1200,
    "height": 800,
    "x": 100,
    "y": 100,
    "alwaysOnTop": true,
    "opacity": 0.9
  },
  "theme": {
    "name": "default"
  },
  "recentFilesLimit": 15
}
```

### 7.2 状态文件（state.json）

- 用于存储历史记录和阅读进度。
- 示例结构：

```json
{
  "recentFiles": [
    "C:/docs/a.md",
    "C:/docs/b.md"
  ],
  "documents": {
    "C:/docs/a.md": {
      "scrollRatio": 0.42,
      "lastOpenTime": "2025-11-15T13:45:00",
      "fileLastModified": 1700000000
    },
    "C:/docs/b.md": {
      "scrollRatio": 0.10,
      "lastOpenTime": "2025-11-16T09:20:00",
      "fileLastModified": 1700001234
    }
  }
}
```

## 8. 编码规范与约束

AI 在输出 C++ 代码时，请遵守以下约定：

- 使用 C++17：
  - 不要使用协程、Concepts、Ranges 等 C++20 之后的特性。
- 不要写 `using namespace std;`。
- 尽量使用智能指针或 Qt 的父子对象机制管理内存，避免裸 `new` / `delete`。
- 头文件中尽量前向声明，减少不必要的包含。
- 命名约定：
  - 类名：`CamelCase`，如 `MainWindow`, `AppState`
  - 成员变量：`m_` 前缀，例如 `m_view`
  - 函数名：`camelCase`
- 对外接口（类和 `public` 函数）保持稳定，不要随意改名，除非确有必要。

## 9. 开发阶段建议

当我给出进一步指令时，请按阶段实现，不要一次性实现所有功能。推荐阶段：

1. 阶段 0：工程骨架（CMake + `main.cpp` + `MainWindow` + 最小 WebEngine 显示）
2. 阶段 1：窗口外观（无边框、透明、置顶、拖动）
3. 阶段 2：Markdown 渲染（本地文件 → HTML → WebEngine）
4. 阶段 3：WebChannel 与内部链接 / 滚动同步
5. 阶段 4：`AppState` / `DocumentState` / 状态持久化（`config.json` + `state.json`）
6. 阶段 5：单实例机制
7. 阶段 6：拖拽打开文件
8. 阶段 7：托盘菜单与最近文件
9. 阶段 8：主题与配置 UI（如需要）
10. 阶段 9：性能与边缘情况处理

在每个阶段内，优先保持代码可编译、可运行，再逐步完善细节。
