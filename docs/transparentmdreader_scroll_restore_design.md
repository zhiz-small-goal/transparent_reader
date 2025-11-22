# TransparentMdReader 无闪烁滚动恢复设计（前端 + Qt 集成）

> 目标：页面加载时**直接落到上次阅读位置**，中间不出现“先在顶部闪一下，再跳到历史位置”的视觉跳动；并且与现有 SQLite `scroll_ratio` 存储 / Qt6 QWebEngine 集成良好配合，方便 Codex Max 按此实现。

---

## 1. 背景与整体思路

### 1.1 背景

- 已有状态库（SQLite）记录每个文档的阅读进度：`scroll_ratio`（0.0–1.0）。
- 启动应用时希望：
  - 自动打开最近一次文档；
  - 并且“无闪烁”恢复到之前的滚动位置。

问题是：如果简单在 Qt 侧 `loadFinished` 后调用 JavaScript 滚动，通常会看到：

1. 页面先以默认位置渲染（顶部）；
2. 再被脚本滚动到目标位置；
3. 肉眼可见“跳一下”，体验不好。

### 1.2 核心思路（概述）

采用“前端预滚动 + 显示延迟 + 一次性应用”的成熟方案：

1. **前端 JS 中增加 `setInitialScroll(ratio)` 接口**：
   - 记录一个 `pendingScrollRatio`；
   - 在 DOMContentLoaded 后，通过 `requestAnimationFrame` 计算目标高度并一次性设置 `scrollTop`。

2. **在预滚动前暂时隐藏内容**：
   - 给 `body` 加一个 `pre-scroll-hidden` 类（或类似标记）；
   - CSS 使用 `visibility: hidden` 隐藏内容区域；
   - 滚动到位后再去掉这个类，让页面一次性“出现在正确位置”。

3. **禁用恢复时的平滑滚动**：
   - CSS 中确保默认滚动是 `scroll-behavior: auto`；
   - JS 调用 `window.scrollTo({ top, behavior: 'instant' })`，避免看到滚动动画。

4. **Qt 侧在 `loadFinished` 后注入滚动比例**：
   - index.html / main.js 里提供全局函数 `setInitialScroll(ratio)`；
   - Qt 通过 `runJavaScript("setInitialScroll(0.123456);")` 或 WebChannel 调用，将 `scroll_ratio` 传入前端；
   - 前端负责在合适的时机完成一次性滚动，然后显示内容。

5. **恢复期间暂停写库**：
   - 前端在恢复滚动时设置 `isRestoringScroll = true`；
   - 滚动事件监听中如果发现正在恢复，直接 return，避免把“恢复过程中的中间值（如 0）”写回 SQLite 覆盖历史。

---

## 2. 前端 CSS 设计

### 2.1 基本滚动行为

在 index.html 的 CSS 中，确保默认滚动为“非平滑”：

```css
html, body {
    scroll-behavior: auto; /* 恢复历史位置时不要平滑滚动 */
}
```

如果需要对部分锚点使用平滑滚动，可以在局部元素上单独设置 `scroll-behavior: smooth`，但恢复历史位置时必须使用 `behavior: 'instant'` 强制禁止动画（见 JS 部分）。

### 2.2 预滚动隐藏类

为“预滚动阶段”准备一个隐藏类，例如：

```css
body.pre-scroll-hidden {
    visibility: hidden;      /* 内容不可见，但仍然参与布局 */
    /* 也可以视情况加 pointer-events: none; */
}
```

说明：

- 使用 `visibility: hidden` 而不是 `display: none`，这样 DOM 会正常布局，可以计算正确的 `scrollHeight`；
- 等滚动到位后，只需移除该类即可让页面在目标位置一次性显示。

---

## 3. 前端 JavaScript 设计

假设主逻辑在 `main.js` 中实现，增加一小段全局脚本支持“无闪烁恢复”。

### 3.1 全局状态与入口函数

新增全局变量和函数：

```js
// 是否已经满足“可以进行初始滚动”的条件（DOM 就绪）
let pageReadyForScroll = false;

// 待应用的初始滚动比例（0.0 ~ 1.0，null 表示没有）
let pendingScrollRatio = null;

// 是否处于“正在恢复历史滚动”的阶段
let isRestoringScroll = false;

// 提供给 Qt/C++ 的入口：设置初始滚动比例
function setInitialScroll(ratio) {
    // 合法性检查与裁剪
    if (typeof ratio !== "number" || isNaN(ratio)) {
        ratio = 0.0;
    }
    if (ratio < 0.0) ratio = 0.0;
    if (ratio > 1.0) ratio = 1.0;

    pendingScrollRatio = ratio;
    tryApplyInitialScroll();
}
```

### 3.2 尝试应用初始滚动

```js
function tryApplyInitialScroll() {
    if (!pageReadyForScroll) return;
    if (pendingScrollRatio == null) return;

    isRestoringScroll = true;

    // 第一帧：根据比例计算 scrollTop
    requestAnimationFrame(() => {
        const docEl     = document.documentElement;
        const viewportH = window.innerHeight || docEl.clientHeight || 0;
        const totalH    = docEl.scrollHeight;
        const maxTop    = Math.max(totalH - viewportH, 0);
        const targetTop = maxTop * pendingScrollRatio;

        // 禁止平滑滚动，使用“瞬时”跳转
        window.scrollTo({
            top: targetTop,
            behavior: "instant" // 忽略 CSS 中的 smooth 设置
        });

        // 第二帧：在内容已经滚到目标位置后再显示页面
        requestAnimationFrame(() => {
            document.body.classList.remove("pre-scroll-hidden");
            isRestoringScroll = false;
        });

        // 清空 pending 标记，防止后续重复滚动
        pendingScrollRatio = null;
    });
}
```

### 3.3 DOMContentLoaded 钩子

在 DOMContentLoaded 时，标记页面已就绪，并尝试应用 pending 滚动：

```js
document.addEventListener("DOMContentLoaded", () => {
    pageReadyForScroll = true;
    // 初始进入页面时隐藏内容，避免用户看到从顶部跳动到历史位置
    document.body.classList.add("pre-scroll-hidden");
    tryApplyInitialScroll();
});
```

说明：

- 如果 Qt 在 DOMContentLoaded 之前就调用了 `setInitialScroll()`，则 `pendingScrollRatio` 会有值，而 `pageReadyForScroll` 为 false，实际滚动等到 DOMContentLoaded 触发后再统一执行。  
- 如果 Qt 在 DOMContentLoaded 之后才调用 `setInitialScroll()`，则 `pageReadyForScroll` 已为 true，`setInitialScroll()` 会立即调用 `tryApplyInitialScroll()`，流程一样。

### 3.4 滚动事件与写库协作

假设当前页面已有滚动事件监听，用于将滚动比例回传 Qt（通过 WebChannel）：

```js
let lastScrollReportTime = 0;
const SCROLL_REPORT_INTERVAL = 200; // ms，示例节流间隔

window.addEventListener("scroll", () => {
    // 恢复历史位置时触发的滚动事件，直接忽略，不写库
    if (isRestoringScroll) return;

    const now = Date.now();
    if (now - lastScrollReportTime < SCROLL_REPORT_INTERVAL) {
        return; // 节流
    }
    lastScrollReportTime = now;

    const docEl     = document.documentElement;
    const viewportH = window.innerHeight || docEl.clientHeight || 0;
    const totalH    = docEl.scrollHeight;
    const maxTop    = Math.max(totalH - viewportH, 1);
    const ratio     = window.scrollY / maxTop;

    // 通过 WebChannel 或其它桥接方式回传给 Qt
    if (window.qt && window.qt.mdReaderBridge) {
        window.qt.mdReaderBridge.reportScrollRatio(ratio);
    }
});
```

这样可以保证：

- 恢复阶段仅由 `setInitialScroll` → `tryApplyInitialScroll` 控制一次性滚动；
- 真正的用户滚动才会触发写库；
- 滚动事件有节流，不会频繁调用 SQLite。

---

## 4. Qt / C++ 集成设计

### 4.1 状态读取：从 SQLite 获取历史 `scroll_ratio`

在 C++ 侧，通过 `StateDbManager`（或等价封装）获取当前要打开的文档滚动比例：

```cpp
double StateDbManager::loadScrollRatio(const QString &normalizedPath)
{
    QSqlQuery q(db_);
    q.prepare(QStringLiteral(
        "SELECT scroll_ratio FROM documents WHERE path = :path"
    ));
    q.bindValue(":path", normalizedPath);
    if (!q.exec()) {
        // log warning
        return 0.0;
    }
    if (q.next()) {
        return q.value(0).toDouble();
    }
    return 0.0;
}
```

### 4.2 文档加载与 `loadFinished`

以 `MainWindow` 为例，当用户打开一个 Markdown 文件时：

1. 规范化路径；
2. 通过 `StateDbManager` 记录“打开事件”（更新 last_open_time 等）；
3. 设置当前文档路径；
4. 调用 WebView 的 `load()` 或加载 index.html + 传入该文档内容。

在 WebView 初始化阶段，连接其 `loadFinished(bool ok)` 信号：

```cpp
connect(m_markdownPage->webView(), &QWebEngineView::loadFinished,
        this, &MainWindow::onWebViewLoadFinished);
```

在槽函数中：

```cpp
void MainWindow::onWebViewLoadFinished(bool ok)
{
    if (!ok) {
        // 记录错误或提示
        return;
    }

    const QString currentPath = m_currentFilePath; // 已经规范化过
    if (currentPath.isEmpty()) {
        return;
    }

    // 从数据库获取历史 scroll_ratio
    const double ratio = m_stateDb->loadScrollRatio(currentPath);

    // 将比例传给前端的 setInitialScroll(ratio)
    QWebEnginePage *page = m_markdownPage->webView()->page();
    if (!page) return;

    const QString js = QStringLiteral("setInitialScroll(%1);")
                           .arg(ratio, 0, 'f', 6); // 保留 6 位小数
    page->runJavaScript(js);
}
```

说明：

- 不需要在 `load()` 之前调用 `runJavaScript`，因为页面尚未构建；  
- 在 `loadFinished` 中调用 `setInitialScroll()` 即可，由前端控制何时真正应用滚动（见 DOMContentLoaded + rAF 组合）；  
- 前端使用 `pendingScrollRatio` + `pageReadyForScroll` 双标记，保证无论调用时机略早或略晚，都能在正确时点进行一次性滚动。

### 4.3 滚动位置回写 SQLite

前端通过 WebChannel 调用类似 `mdReaderBridge.reportScrollRatio(double ratio)` 的方法：

```cpp
class MdReaderBridge : public QObject
{
    Q_OBJECT
public:
    explicit MdReaderBridge(StateDbManager *stateDb, QObject *parent = nullptr)
        : QObject(parent), m_stateDb(stateDb) {}

public slots:
    void reportScrollRatio(double ratio) {
        if (!m_stateDb) return;
        if (ratio < 0.0) ratio = 0.0;
        if (ratio > 1.0) ratio = 1.0;

        // 可以做节流：例如记录最后一次更新时间戳，再决定是否立即写入
        m_stateDb->updateScrollRatio(m_currentPath, ratio);
    }

    void setCurrentPath(const QString &normalizedPath) {
        m_currentPath = normalizedPath;
    }

private:
    StateDbManager *m_stateDb = nullptr;
    QString         m_currentPath;
};
```

SQLite 写入示例：

```cpp
void StateDbManager::updateScrollRatio(const QString &normalizedPath, double ratio)
{
    QSqlQuery q(db_);
    q.prepare(QStringLiteral(
        "UPDATE documents SET scroll_ratio = :ratio WHERE path = :path"
    ));
    q.bindValue(":ratio", ratio);
    q.bindValue(":path",  normalizedPath);
    q.exec(); // 可根据需要检查返回值并记录日志
}
```

> 注：实际实现中可以在 `MdReaderBridge` 层再做一次 QTimer 节流（例如每 500ms 批量写一次），避免频繁 UPDATE。

---

## 5. 时序总结（从“打开文件”到“落到历史位置”）

以“启动应用 → 自动打开上次文件”场景为例：

1. 应用启动，初始化 QSettings、UI、StateDbManager；
2. 读取“最近一次文件”的路径与 `scroll_ratio`；
3. MainWindow 调用 `openFile(path)`，设置当前路径并触发 WebView 加载 index.html；
4. 浏览器内部构建 DOM，触发 `DOMContentLoaded`：
   - JS 中：`pageReadyForScroll = true;`，并给 body 加 `pre-scroll-hidden`；
   - 调用 `tryApplyInitialScroll()`（此时可能还没有 `pendingScrollRatio`）。
5. Qt 收到 `loadFinished(true)` 信号：
   - C++ 查询 state.db，得到 `scroll_ratio`；
   - 调用 `page->runJavaScript("setInitialScroll(ratio);")`；
   - JS：记录 `pendingScrollRatio` 并再次调用 `tryApplyInitialScroll()`。
6. `tryApplyInitialScroll()` 检查到：
   - `pageReadyForScroll == true` 且 `pendingScrollRatio != null`；
   - 使用两层 `requestAnimationFrame`：
     - 第一帧：根据比例计算目标 `top`，使用 `scrollTo({behavior:'instant'})` 直接跳转；
     - 第二帧：移除 `pre-scroll-hidden`，页面在正确位置显示。
7. 后续用户滚动：
   - `isRestoringScroll == false`，滚动事件监听开始生效；
   - 每隔一小段时间（节流）将新的 `scroll_ratio` 回传 Qt，写入 SQLite。

整个过程对用户来说是：

- 打开应用 → 直接看到上次阅读位置的页面，没有经历“先到顶部再跳下去”的过程。

---

## 6. 给 Codex Max 的实施任务清单

**前端（main.js / index.html）：**

1. 在 CSS 中：
   - 为 `html, body` 设置 `scroll-behavior: auto`；
   - 添加 `body.pre-scroll-hidden { visibility: hidden; }`。

2. 在 JS 中：
   - 增加全局变量：`pageReadyForScroll`、`pendingScrollRatio`、`isRestoringScroll`；
   - 实现 `setInitialScroll(ratio)` 和 `tryApplyInitialScroll()`；
   - 在 `DOMContentLoaded` 中标记 `pageReadyForScroll = true`，给 `body` 加 `pre-scroll-hidden` 并调用 `tryApplyInitialScroll()`；
   - 在现有滚动上报逻辑中，加入 `if (isRestoringScroll) return;` 和节流。

**Qt / C++ 侧：**

3. 在 `StateDbManager` 中：
   - 实现 `loadScrollRatio(path)` 与 `updateScrollRatio(path, ratio)`。

4. 在 `MainWindow` / `MarkdownPage` 中：
   - 打开文档时记录当前规范化路径；
   - WebView `loadFinished(true)` 时：
     - 通过 `StateDbManager::loadScrollRatio()` 获取历史值；
     - 构造 JS 调用：`setInitialScroll(ratio)` 并执行 `runJavaScript(...)`。

5. 在 WebChannel bridge 中：
   - 提供 `reportScrollRatio(double)`，将前端上报的比例路由到 `StateDbManager::updateScrollRatio()`；
   - 提供 `setCurrentPath(QString)` 以便 bridge 知道当前文档是哪一个。

按以上设计落地后，即可实现：

- 启动应用 / 打开文件时，页面在第一次显示时就处于历史阅读位置；
- 无闪烁、无平滑滚动动画；
- 滚动进度稳定写入 SQLite，配合“自动打开上次文件”功能形成完整的记忆模块。
