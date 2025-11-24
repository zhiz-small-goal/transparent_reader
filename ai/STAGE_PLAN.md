# 阶段计划

---

## 阶段 0：工程骨架（最小可运行窗口）

日期：2025-11-16
状态：已完成

目标：
- 用 Qt 6 + CMake 搭一个最小可运行的项目。
- 程序启动后显示一个主窗口（暂时可以是普通窗口，不要求透明）。
- 主窗口里有一个 QWebEngineView，加载本地一个简单的 index.html。

涉及文件（由 AI 生成）：
- CMakeLists.txt
- src/CMakeLists.txt
- src/app/main.cpp
- src/app/MainWindow.h
- src/app/MainWindow.cpp
- resources/web/index.html（简单占位页面）

---

## 阶段 1：窗口外观（无边框+透明+置顶+可移动）

日期：2025-11-16
状态：已完成

目标：
- 让 MainWindow 改成无边框 + 可拖动 + 默认置顶
- 让窗口支持整体透明度（例如 0.9），边缘也是透明的
- 设定默认大小 720x900、最小大小 480x600
- 不引入锁定模式 / 托盘 / 状态持久化，只改窗口外观相关代码

涉及文件：
- D:/zhiz-c++/transparent_reader/src/app/main.cpp
- D:/zhiz-c++/transparent_reader/src/app/MainWindow.cpp
- D:/zhiz-c++/transparent_reader/src/app/MainWindow.h


---

## 阶段 2：添加标题栏以及常规按钮

日期：2025-11-16
状态：已完成

目标：
- 在无边框窗口顶部添加自定义标题栏，整体透明度与窗口一致。
- 标题栏与内容区域之间添加一条若有若无的分割线（浅色半透明）。
- 在标题栏右侧添加常规按钮：最小化（−）、锁定（🔒）、设置（⚙）、关闭（×）。
- 标题栏空白区域支持按住左键拖动整个窗口。

涉及文件：
- D:/zhiz-c++/transparent_reader/src/app/MainWindow.h
- D:/zhiz-c++/transparent_reader/src/app/MainWindow.cpp


---


## 阶段 3：添加标题栏以及常规按钮

日期：2025-11-16
状态：已完成

目标：
- 在无边框窗口顶部添加自定义标题栏，整体透明度与窗口风格保持一致。
- 标题栏与 Web 内容区域之间添加一条若有若无的分割线。
- 在标题栏右侧添加常规按钮：最小化（−）、锁定（🔓/🔒）、设置（⚙）、关闭（×）。
- 标题栏空白区域支持拖动窗口；锁定按钮可以切换「允许拖动 / 禁止拖动」状态。
- 设置按钮暂时弹出占位对话框，后续接入真正的设置界面。

涉及文件：
- D:/zhiz-c++/transparent_reader/src/app/MainWindow.h
- D:/zhiz-c++/transparent_reader/src/app/MainWindow.cpp

---

## 阶段 4：最小 Markdown 打开功能（Ctrl+O）

日期：2025-11-16
状态：已完成

目标：
- 支持通过快捷键 Ctrl+O 打开本地 Markdown 文件。
- 从文件系统读取 .md 文本（按 UTF-8 解码）。
- 将 Markdown 文本转换为简单 HTML 并在 QWebEngineView 中显示（当前只做行内换行，后续替换为正式 Markdown 渲染管线）。
- 暴露 openMarkdownFile(const QString& path) 作为统一入口，后续拖拽、托盘菜单、单实例等入口都复用该函数。

涉及文件：
- D:/zhiz-c++/transparent_reader/src/app/MainWindow.h
- D:/zhiz-c++/transparent_reader/src/app/MainWindow.cpp

---

## 阶段 5：Markdown 打开功能（快捷键 + 目录记忆）

日期：2025-11-16
状态：已完成

目标：
- 支持通过快捷键 Ctrl+O 打开本地 Markdown 文件。
- 使用 UTF-8 读取 .md 文本，并通过 basicMarkdownToHtml 转成简单 HTML，在 QWebEngineView 中显示。
- 内容区域采用深色半透明背景 + 浅色文字，保证在透明窗口下可读。
- 提供统一入口：`MainWindow::openMarkdownFile(const QString& path)`，后续拖拽 / 最近文件 / 单实例等入口复用该函数。
- 会话内记住“最后一次成功打开的目录”，多次 Ctrl+O 时从该目录起跳。
- 使用 `QSettings("zhiz", "TransparentMdReader")` 持久化保存 `ui/lastOpenDir`，支持跨重启记住上次打开目录。
- 打开文件后，窗口标题更新为 `TransparentMdReader - <文件名>`，方便区分当前文档。

涉及文件：
- D:/zhiz-c++/transparent_reader/src/app/MainWindow.h
- D:/zhiz-c++/transparent_reader/src/app/MainWindow.cpp

---


# 阶段 6：Markdown 链接渲染与点击行为（简易版）

日期：2025-11-16  
状态：已完成

目标：
- 在现有的 C++ 端简易 Markdown → HTML 管线中，增加对 `[文本](链接)` 语法的支持。
- 为 `<a>` 链接添加基础样式：浅蓝色文字、下划线、hover 透明度变化，在透明窗口上也有清晰的可点击提示。
- 引入自定义 `MarkdownPage`（继承 `QWebEnginePage`），拦截所有链接点击：
  - `.md` / `.markdown` 相对链接 → 交由主窗口在阅读器中打开对应 Markdown 文件。
  - 其它 `http(s)` 等外部链接 → 使用系统默认浏览器打开。
- 在 `MainWindow` 中记录当前已打开的 Markdown 文件绝对路径，用于解析内部相对链接。
- 保持实现尽量简单，为后续切换到「真正前端渲染管线（marked.js + WebChannel）」做铺垫，不破坏整体架构。

涉及文件：
- D:/zhiz-c++/transparent_reader/src/app/MainWindow.h
- D:/zhiz-c++/transparent_reader/src/app/MainWindow.cpp
- D:/zhiz-c++/transparent_reader/src/app/MarkdownPage.h
- D:/zhiz-c++/transparent_reader/src/app/MarkdownPage.cpp

---

# 阶段 7：Markdown 链接跳转（内部 .md + 外部浏览器）

日期：2025-11-16
状态：已完成

目标：
- 支持在预览中点击 Markdown 链接：
  - 内部 `.md` / `.markdown` 链接：在阅读器中打开对应本地 Markdown 文件。
  - 其它 `http` / `https` 等外部链接：交给系统默认浏览器打开。
- 在 MainWindow 中记录当前已打开的 Markdown 文件绝对路径，用于解析相对链接（例如 `../chapter/xxx.md`）。
- 当链接指向的本地文件不存在时，给出友好的错误提示，而不是静默失败。

涉及文件：
- D:/zhiz-c++/transparent_reader/src/app/MainWindow.h
- D:/zhiz-c++/transparent_reader/src/app/MainWindow.cpp
- D:/zhiz-c++/transparent_reader/src/app/MarkdownPage.h
- D:/zhiz-c++/transparent_reader/src/app/MarkdownPage.cpp


---

### 2025-11-17 接入 markdown-it 渲染并修复 Markdown 链接解析

**涉及阶段：**  
- 阶段 6（Markdown 渲染加强）
- 阶段 7（链接与图片交互）

**触发原因：**
- 手写的 Markdown 解析器功能有限，且在切到前端渲染后，图片等相对链接会被解释为相对于 `index.html`，导致路径错误；希望对齐 VS Code 等编辑器的成熟方案，同时保留图片浮层交互。

**改动概览：**
- `resources/web/index.html`
  - 新增本地 `markdown-it.min.js` 的 `<script>` 引用，为前端提供标准 Markdown 解析能力。
- `resources/web/main.js`
  - 改为使用 markdown-it 渲染 Markdown 文本。
  - 通过自定义 `link_open` / `image` 渲染规则，将所有相对链接转换为基于当前 md 所在目录（baseUrl）的绝对 `file://` 地址。
  - 保留并集成图片浮层（Lightbox）逻辑，实现点击图片链接在当前页面模态预览、支持关闭按钮 / 背景点击 / Esc 退出，并在加载失败时提示。
- `src/app/MainWindow.h`
  - 将 `renderMarkdownInPage` 扩展为接受 `baseUrl` 参数。
  - 新增 `m_pendingBaseUrl` 字段，用于在 Web 页面尚未加载完成时暂存文档的 baseUrl。
- `src/app/MainWindow.cpp`
  - 在构造函数中 `loadFinished` 回调中，加载完成后使用 `m_pendingBaseUrl` 调用新版 `renderMarkdownInPage`，并清理缓存。
  - 重写 `renderMarkdownInPage`，调用前端 `window.renderMarkdown(markdown, title, baseUrl)`。
  - 重写 `openMarkdownFile`：
    - 当启用嵌入式前端时，计算 md 所在目录的 `file:///.../` 作为 baseUrl，页面加载完成后调用前端渲染；否则退回旧的 `basicMarkdownToHtml + setHtml` 渲染路径。

**关键点说明：**
- 链接解析策略统一为“相对于当前 Markdown 文件目录”，和 GitHub / VS Code 等工具的行为一致，确保 `media/xxx.png` 等路径在项目移动后仍然可用。
- 前端渲染完全依赖 markdown-it，后续可以方便地加表格、任务列表等扩展，而无需在 C++ 中维护复杂的解析逻辑。
- 旧的纯 HTML 渲染路径仍然保留为兜底方案，当找不到 `index.html` 或前端初始化失败时不会影响基本阅读能力。

**测试验证：**
- [ ] 打开包含 `[xxx](media/xxx.png)` 和 `![](media/xxx.png)` 的 Markdown，确认图片以浮层形式弹出并支持三种关闭方式。
- [ ] 将某张图片重命名或删除，点击对应链接时，确认弹出“图片加载失败或文件不存在”，路径指向 md 同级的 `media` 目录。
- [ ] 打开包含内部 `.md` 链接和外部 `http/https` 链接的文档，确认内部跳转仍由应用处理，外部链接交给系统浏览器，并能正常返回当前文档。
- [ ] 在应用启动后立即打开 md（index.html 尚未完全加载），确认页面加载完成后能正确显示内容，且链接/图片路径解析正确。

**后续 TODO：**
- [ ] 将图片加载失败时的 `alert` 替换为页面内非阻断式提示（toast），进一步优化阅读体验。
- [ ] 视需求接入 markdown-it 的表格 / 任务列表等插件，对 LearnCpp 文档的渲染效果做专项调优。

---

### 2025-11-17 记住上次打开的 Markdown 目录

**涉及阶段：**  
- 阶段 5（基础交互优化）

**触发原因：**
- 每次启动应用后，文件对话框默认始终停在“文档”目录，无法记住上一次成功打开 `.md` 所在的目录，影响使用效率。

**改动概览：**
- `src/app/MainWindow.cpp`
  - 在 `MainWindow::openMarkdownFile(const QString &path)` 中，在成功解析 `QFileInfo` 并更新 `m_lastOpenDir` 后，使用 `QSettings("zhiz", "TransparentMdReader")` 将 `ui/lastOpenDir` 写入配置，使得下次启动时构造函数能从配置恢复最近打开目录。

**关键点说明：**
- 构造函数依然通过 `QSettings` 读取 `ui/lastOpenDir`，并在该路径存在时用作 `m_lastOpenDir`，否则退回到文档目录或用户 Home 目录。
- 本次修改只增加了写入逻辑，不改变原有 Markdown 渲染路径（无论是 basicMarkdownToHtml 还是嵌入式前端 markdown-it 渲染），不会影响已有的链接、图片等行为。

**测试验证：**
- [ ] 启动应用后，使用 Ctrl+O 打开任意目录下的 `.md` 文件，关闭应用并重新启动，确认文件对话框默认目录为刚才打开的目录。
- [ ] 修改为另一个目录再打开 `.md`，重新启动后确认默认目录跟随最新一次打开的目录。
- [ ] 在目标目录被删除或移动后启动应用，确认程序能正常回退到文档目录或用户 Home 目录，不崩溃。

**后续 TODO：**
- [ ] 视需求扩展记忆内容，例如记住“最近打开文件列表”或“每个文件的滚动位置”，统一放入状态持久化模块管理。

---

### 2025-11-17 统一 Markdown 内部链接与对话框打开逻辑

**涉及阶段：**  
- 阶段 5（基础交互与状态）
- 阶段 6（Markdown 渲染）
- 阶段 7（链接与跳转）

**触发原因：**
- 程序启动后首次打开的 Markdown 文档可以正常渲染并通过文内链接跳转，但在跳转到第二篇文档后，再次点击文内 `.md` 链接不再跳转；使用 Ctrl+O 选择其它文件时，界面仍停留在首次通过链接打开的文档。

**改动概览：**
- `src/app/MainWindow.cpp`
  - `openMarkdownFileFromDialog()`
    - 确认通过 `QFileDialog::getOpenFileName` 选择文件后统一调用 `openMarkdownFile(path)`，使用 `m_lastOpenDir` 作为初始目录。
  - `handleOpenMarkdownUrl(const QUrl &url)`（重写）
    - 简化为仅处理内部 `.md` 链接：从 `url.path()` 提取文件名，以 `m_currentFilePath` 所在目录为基准拼出目标绝对路径，若文件存在则调用 `openMarkdownFile()` 打开。
    - 移除对 `url.isLocalFile()` / `isRelative()` / `http(s)` 等复杂分支判断，避免误判为“当前文件”或“非 md 链接”提前返回。
  - `openMarkdownFile(const QString &path)`（重写）
    - 每次成功打开时更新 `m_lastOpenDir` 和 `m_currentFilePath`，并通过 `QSettings("zhiz", "TransparentMdReader")` 持久化 `ui/lastOpenDir`。
    - 使用 `basicMarkdownToHtml` 将 Markdown 转为 HTML，并以 `QUrl::fromLocalFile(path)` 作为 `baseUrl` 调用 `m_view->setHtml(...)`，保持相对链接（图片 / 内部链接）解析正确。
    - 移除任何“重复文件不再渲染”的优化逻辑，确保每次调用都重新渲染当前文档。

**关键点说明：**
- 现在“通过 Ctrl+O 打开文档”和“通过文内链接跳转到其它文档”统一走 `openMarkdownFile()`，保证当前文档路径与最后打开目录始终保持最新。
- Markdown 内部链接解析策略统一为“当前文档所在目录 + 链接目标文件名”，不再依赖 QWebEngine 生成的完整 `file:///` 路径细节，避免不同跳转路径下 URL 形式不一致导致的误判。

**测试验证：**
- [ ] 启动应用，通过 Ctrl+O 打开 A.md，确认渲染正常、链接高亮存在。
- [ ] 在 A.md 内点击指向 B.md 的链接，确认成功跳转到 B，窗口标题与内容更新。
- [ ] 在 B.md 内点击指向 C.md 或返回 A.md 的链接，确认可以连续跳转多次。
- [ ] 在任意时刻使用 Ctrl+O 打开新的 md，确认界面切换到所选文档，不再停留在之前通过链接打开的文件。
- [ ] 点击文内 http/https 链接时，确认由默认浏览器打开，阅读器仍停留在当前文档。

**后续 TODO：**
- [ ] 在该基础上继续接入前端 markdown-it 渲染管线，将 `openMarkdownFile()` 的渲染部分替换为调用 `renderMarkdownInPage()`，统一走 index.html + main.js 的前端方案。
- [ ] 在状态持久化模块中进一步扩展“最近打开文档列表”和“滚动位置记忆”等功能。

---

## 阶段 6：2025-11-21 新增托盘菜单与自启/日志开关

日期：2025-11-21
状态：已完成

目标：
- 托盘集中提供打开文件、自启切换、日志记录和退出入口
- 自启状态按系统实际注册表同步，日志开关可即时生效
- 托盘左/双击可快速唤起主窗口

涉及文件：
- F:/zhiz-c++/transparent_reader/src/app/MainWindow.cpp
- F:/zhiz-c++/transparent_reader/src/app/MainWindow.h

改动概览：

- MainWindow.cpp：新增自启/日志辅助函数（注册表读写、文件日志 handler），初始化时创建托盘；实现托盘菜单与点击行为；处理自启/日志勾选的状态同步与错误提示，退出时清理日志 handler。
- MainWindow.h：加入托盘成员与槽声明，记录自启、日志开关状态。

关键点说明：

- 自启仅在 Windows 使用 HKCU\\...\\Run 写入当前可执行路径，失败时回退勾选并提示。
- 日志记录通过全局 message handler 追加到 AppData 路径下 transparent_reader.log，关闭时恢复原消息处理器避免泄漏。
- 托盘仅在系统支持托盘时创建，左/双击托盘会显示并激活主窗口。
---

## 阶段 6：2025-11-21 新增托盘菜单与自启/日志开关

日期：2025-11-21  
状态：已完成

目标：
- 托盘集中提供打开文件、自启切换、日志记录和退出入口
- 自启状态按系统实际注册表同步，日志开关可即时生效
- 托盘左/双击可快速唤起主窗口

涉及文件：
- F:/zhiz-c++/transparent_reader/src/app/MainWindow.cpp
- F:/zhiz-c++/transparent_reader/src/app/MainWindow.h

改动概览：
- MainWindow.cpp：新增自启/日志辅助函数（注册表读写、文件日志 handler），初始化时创建托盘；实现托盘菜单与点击行为；处理自启/日志勾选的状态同步与错误提示，退出时清理日志 handler。
- MainWindow.h：加入托盘成员与槽声明，记录自启、日志开关状态。

关键点说明：
- 自启仅在 Windows 使用 HKCU\...\Run 写入当前可执行路径，失败时回退勾选并提示。
- 日志记录通过全局 message handler 追加到 AppData 路径下的 transparent_reader.log，关闭时恢复原消息处理器避免泄漏。
- 托盘仅在系统支持托盘时创建，左/双击托盘会显示并激活主窗口。

---

### 2025-11-22 翻页按钮失效修复

- 现象：标题栏翻页按钮（上一屏/下一屏）点击无效，滚轮翻页正常。
- 原因：翻页逻辑仅调用 `window.scrollBy` 未命中实际可滚动容器；隐藏滚动条时容易阻断滚动。
- 方案：在 `MainWindow::scrollPageUp/Down` 中使用 JS 依次尝试 `scrollingElement` / `documentElement` / `body` / `#md-root` / `.md-root` / `.markdown-body`，找到可滚动容器后调整 `scrollTop`，无命中时回退 `window.scrollBy`。
- 状态：已完成

### 2025-11-22 引入SQLite状态库骨架

- 目标：使用 SQLite 统一持久化最近文件、阅读进度等文件级状态，铺设后续历史记录功能。
- 涉及文件：
  - src/app/StateDbManager.h
  - src/app/StateDbManager.cpp
  - src/CMakeLists.txt
  - src/app/main.cpp
  - src/app/MainWindow.cpp
- 改动概览：
  - 新增 StateDbManager 单例，负责 state.db 的打开、PRAGMA 设置、schema 初始化（app_meta + documents）、接口封装（recordOpen、updateScroll、loadScroll、listRecent）。
  - CMake 增加 Qt6::Sql 依赖，编译新文件；main.cpp 设置 Organization/ApplicationName 以使用标准 AppData 路径。
  - MainWindow 构造时初始化状态库，openMarkdownFile 成功后写入文件 mtime/size 和 last_open_time 到 SQLite。
- 状态：已完成

### 2025-11-22 接入SQLite阅读进度

- 目标：将阅读进度持久化到 SQLite，并在打开文档时恢复。
- 涉及文件：
  - src/app/StateDbManager.h
  - src/app/StateDbManager.cpp
  - src/app/MainWindow.h
  - src/app/MainWindow.cpp
- 改动概览：
  - 新增滚动比率读取/写入接口使用：打开文件后从 SQLite 读取进度并应用到页面；滚动时定时读取页面滚动比例并写入数据库。
  - 增加 `applyScrollRatio` 和定时器，优先命中实际滚动容器（scrollingElement/#md-root/.markdown-body），找不到则回退到 window.scroll。
- 状态：已完成

### 2025-11-22 历史记录持久化与上限设置

- 目标：历史前进/后退跨重启保持有效，并可配置上限，避免无限增长。
- 涉及文件：
  - src/app/MainWindow.h
  - src/app/MainWindow.cpp
- 改动概览：
  - 历史列表持久化到 QSettings，启动加载；新增默认上限 20，添加设置对话框输入（1~200）。
  - 新增 `trimHistory()` 保证列表不超过上限，持久化时同步保存上限。
  - ReaderSettingsDialog 增加“历史记录上限”输入，变更即裁剪并保存。
- 状态：已完成
- - - 
 
 # # #   2 0 2 5 - 1 1 - 2 3   �S�SN�n�R��c�g��N�[MO	�
 
 -   9hnc(u7b�S���h�g�S�S�[*�N�n�RMOnb`Y�*g�O9e�Nx0
 -   �[MOMR�z:\  ` s e t I n i t i a l S c r o l l `   �[�s��[�  S Q L i t e   -N�]	g�n�R��U_FOb`Y�e�eHe�v^�_�_���[�ehV�V�Q:N  0 0
 -   �[MO�Q�  ` . m d `   ���c㉐g�N�OYu�e�NT�P[�vU_/ �v�[_�v���cO����㉐g0RS_MR�vU_�[� �e�NNX[(W 0
 -   �[MOꁨRSb _ gя�e�N�e��Y�g g�e��U_�]:1YO�v�cԏ�V�T�~	gHe��U_NO\Ջ0 
 # # #   2 0 2 5 - 1 1 - 2 3   �n�Rb`YN�S�S�[*��OY�[�e
 
 -   MR�z�e�X  ` s e t I n i t i a l S c r o l l ` �D O M   1\�~T&^͑Ջ0Wb`Y�n�R�ԏ�V<P�~  C + +   $R�e/f&T�zsSuHe0
 -   C + +   �te�n�Rb`YI{�_�z�S��MQb`Y*g�[b�e���[�ehV�Q�V  0 0
 -   �Q�  ` . m d `   ���c㉐g9e:N�W�NS_MR�vU_�v�[te�v�[_��OYuP[�vU_�T  ` . . ` 0
 -   /T�RꁨRSb _ gя�e�N�e�:1Y��U_O�Ǐv^�~�~\ՋT�~y�0 
 # # #   2 0 2 5 - 1 1 - 2 4   �S�S�R}�N�n�Rb`Y�Ock��[�e-N	�
 
 -   �g ��Qpe�N�(u N!k  ` l o a d H i s t o r y F r o m S e t t i n g s ` ��MQ͑Y���v�S�Sh0
 -   ` a p p l y S c r o l l R a t i o `   �X�R͑Ջ핯s�b�RMR�Oc  ` m _ r e s t o r i n g S c r o l l ` �2�bkǑ7h�Q�V  0 0 
 # # #   2 0 2 5 - 1 1 - 2 4   �n�RN�S�S�OY��~�~	�
 
 -   o p e n M a r k d o w n F i l e ��^Tnt  m _ o p e n i n g F i l e �p e n d i n g   R/eN�Q�QGPpenc�2n�gT�Qb`Y�n�R�v^:N  p e n d i n g   R/eX[  s c r o l l   r a t i o 0
 -   a p p l y S c r o l l R a t i o ��[�ehV(W  o p e n i n g / r e s t o r e   g��N�Q�n�R0 
 ### 2025-11-24 会话历史恢复检查

- 复现用户反馈：依次打开 A、B、C，后退到 B 并退出，重启后历史只剩 A、B，无法前进到 C。
- 定位：启动流程 loadHistoryFromSettings() 先恢复列表与索引（例如 [A,B,C]，index=1），随即 autoOpenLastFileIfNeeded() 默认打开最近文档，调用 openMarkdownFile(..., addToHistory=true) 会截断 m_history = m_history.mid(0, m_historyIndex+1)，导致 C 被丢弃。
- 建议修复：自动打开最近文件时禁用历史截断（传 addToHistory=false 或复用当前索引而不修改栈），确保上/下一篇链条跨重启完整。
### 2025-11-24 会话历史恢复修复
- 启动流程自动打开最近文件时改为 openMarkdownFile(..., addToHistory=false)，避免截断已有的上/下一篇历史栈。
- 仅调整调用参数，不影响滚动位置存储及其它行为，风险低。
### 2025-11-24 托盘最近打开菜单
- 托盘新增“最近打开”子菜单，读取 SQLite 最近列表（默认上限20，可调），点击可直接打开对应文件。
- 设置对话框增加“最近打开上限”输入，QSettings 持久化到 recent/limit，修改后即时重建托盘菜单。
- 托盘子菜单支持“清空最近列表”，仅将 last_open_time 置 0，不触碰滚动进度等其他字段；同时过滤缺失文件并标记 markMissing。
- 打开文件成功后自动刷新托盘最近菜单，确保列表实时更新。
### 2025-11-24 托盘驻留关闭行为
- 覆写 closeEvent：在系统托盘可用时，窗口关闭改为隐藏，不退出进程，托盘常驻可单击/双击唤回。
- 托盘“退出”动作设置 m_exiting 标志后再 QApplication::quit()，确保真正退出时绕过隐藏逻辑。
### 2025-11-24 托盘重开标题栏修复
- 覆写 showEvent/hideEvent：窗口通过托盘恢复时自动同步并显示浮动标题栏，隐藏时一起隐藏，避免关闭后再打开看不到标题栏。
### 2025-11-24 托盘单击显示/隐藏
- 托盘点击（单击/双击）改为切换显示/隐藏：若窗口隐藏或最小化则恢复显示并前置，否则隐藏，便于托盘快速收起/唤醒。
### 2025-11-24 背景透明度可设为 0
- 调整设置读取时的夹值下限，允许 backgroundOpacity 读取到 0.0，实现完全透明背景。
### 2025-11-24 窗口不透明度调整
- 将 MainWindow 默认窗口不透明度固定为 1.0，避免整体透明导致文字消失，让背景透明度完全由前端样式控制。
### 2025-11-24 背景透明兜底
- applyReaderStyle 在背景透明度为 0 时强制使用 rgba(0,0,0,0)，并为 body/md-root 背景加 !important，避免重启后残留底色。
### 2025-11-24 背景透明边框/阴影同步
- style.css 为边框/阴影引入变量（--md-border/--md-shadow），默认值保持原样。
- applyReaderStyle：背景透明度为 0 时同时将边框、阴影设为全透明；注入 CSS 增加这些变量并对 md-root 使用 !important 覆盖默认。
### 2025-11-24 背景透明度渐变同步边框/阴影
- 边框与阴影透明度随背景透明度线性衰减（0.18x/0.45x），避免从较高透明度降到 0 时残留灰色描边/阴影。
### 2025-11-24 滚动条与锁定持久化
- 启动时读取 reader/showScrollbar 与 reader/manualLocked，应用到样式与锁定状态。
- 切换锁定时立即写入 reader/manualLocked，保持跨重启偏好；滚动条开关已由样式回调写入。
### [编号占位：例如 006] 2025-11-24 窗口尺寸提示与代码块换行

日期：2025-11-24  
状态：已完成  

**变更类型：**  
- 行为调整 / Bug 修复  

**涉及阶段：**  
- （未区分阶段）  

**目标：**  
- 提升窗口宽高设置的可理解性和可用性，超出范围时提供明确反馈  
- 让 Markdown 代码块在超长行时可自动换行，避免水平滚动  

**触发原因：**  
- 调整窗口尺寸时输入超出下限/上限会被静默夹紧，用户感觉“无效”  
- 代码块超长行无法换行影响阅读  

**涉及文件：**  
- src/app/MainWindow.cpp  
- resources/web/style.css  

**改动概览：**  
- ReaderSettingsDialog：窗口宽高 spinbox 增加单位/提示/步进，统一使用窗口尺寸常量，输入超界时自动回显夹紧结果，并新增范围提示标签  
- MainWindow：窗口尺寸初始化与写回均使用统一的上下限常量，确保一致的边界  
- style.css：代码块 pre code 启用 pre-wrap 与 overflow-wrap:anywhere 以支持自动换行  

**关键点说明：**  
- 窗口尺寸有效范围：宽 480–3840 px，高 600–2160 px，超界自动夹紧并在提示中说明  
- 代码块换行仅影响超长行显示，仍保留等宽字体与原有样式  

**测试验证：**  
- [ ] 在设置对话框输入小于/大于范围的宽高，确认数值被自动调整并显示范围提示  
- [ ] 调整窗口尺寸后重启应用，确认尺寸被持久化  
- [ ] 打开含超长代码行的 Markdown，确认代码块自动换行且未出现水平滚动条  

**后续 TODO：**  
- [ ] 按需增加“代码块自动换行”开关，让用户可切换行为  
### [编号占位：例如 007] 2025-11-24 代码块背景可调与窗口尺寸提示优化

日期：2025-11-24  
状态：已完成  

**变更类型：**  
- 功能新增 / 行为调整  

**涉及阶段：**  
- （未区分阶段）  

**目标：**  
- 代码块背景颜色/透明度可在设置中调整，可设为完全透明并去掉边框残留  
- 窗口尺寸上下限提示更清晰，输入超界时即时反馈  

**触发原因：**  
- 需求：代码块背景希望与正文背景一样可控，支持全透明且无边框  
- 需求：窗口尺寸输入超出范围时希望明确反馈而非静默无效  

**涉及文件：**  
- src/app/MainWindow.cpp  
- resources/web/style.css  
- ai/STAGE_PLAN.md  

**改动概览：**  
- ReaderStyle 新增 codeBackgroundColor/codeBackgroundOpacity，QSettings 读写并在 applyReaderStyle 注入 `--md-code-bg/--md-code-border`（透明时边框全透明）  
- ReaderSettingsDialog 增加代码块背景色按钮与透明度滑条，范围 0~100%，即时生效并持久化  
- CSS 代码块 `pre code` 保持使用变量，开启自动换行（pre-wrap + overflow-wrap:anywhere）配合可透明背景  

**关键点说明：**  
- 代码块背景默认黑色 70% 透明，可调为 0% 完全透明，边框随透明度线性衰减至全透明  
- 窗口尺寸有效范围宽 480–3840，高 600–2160，超界自动夹紧并提示  

**测试验证：**  
- [ ] 设置中调节代码块背景色/透明度，确认代码块即时更新，可设为 0% 完全透明无边框  
- [ ] 保存后重启，代码块背景配置保留  
- [ ] 窗口尺寸输入超界被自动夹紧且提示语展示范围  

**后续 TODO：**  
- [ ] 如有需要，为代码块背景添加单独的“边框开关”  
### [编号占位：例如 008] 2025-11-24 代码块背景颜色生效修复

日期：2025-11-24  
状态：已完成  

**变更类型：**  
- Bug 修复  

**涉及阶段：**  
- （未区分阶段）  

**目标：**  
- 确保代码块背景颜色设置能被正确应用到页面样式

**触发原因：**  
- 用户反馈代码块背景颜色选项无效，只有透明度生效

**涉及文件：**  
- src/app/MainWindow.cpp  
- ai/STAGE_PLAN.md  

**改动概览：**  
- applyReaderStyle 的 CSS 注入改为链式 arg，确保 `--md-code-bg/--md-code-border` 占位符正确替换
- 代码块背景颜色选择对话框增加 colorSelected 信号处理，保证点击确定后也写入样式并刷新按钮

**关键点说明：**  
- 代码块背景颜色/透明度均通过 `--md-code-bg` 注入，透明时边框随之全透明

**测试验证：**  
- [ ] 在设置里修改代码块背景颜色后立即刷新，颜色应按选取值生效
- [ ] 点击“确定”后关闭对话框，再次打开确认颜色已被记住
- [ ] 调整透明度为 0 时背景与边框均透明

**后续 TODO：**  
- 无  
### [编号占位：例如 009] 2025-11-24 移除代码块颜色自定义，保留透明度

日期：2025-11-24  
状态：已完成  

**变更类型：**  
- 行为调整 / Bug 修复  

**涉及阶段：**  
- （未区分阶段）  

**目标：**  
- 取消代码块背景颜色自定义功能，仅保留透明度调节，恢复默认配色

**触发原因：**  
- 代码块背景颜色选项持续无效，决定简化功能只支持透明度

**涉及文件：**  
- src/app/MainWindow.cpp  
- resources/web/style.css  
- ai/STAGE_PLAN.md  

**改动概览：**  
- 移除代码块背景颜色字段、配置项和 UI 按钮，保留透明度滑条
- applyReaderStyle 固定代码块基色为黑色，透明度可调，边框随透明度衰减至全透明
- 清理 QSettings 读写中与颜色相关的键，避免无效配置
- 保持默认 CSS 变量 `--md-code-bg` 作为初始值

**关键点说明：**  
- 代码块颜色固定为黑色，仅透明度 0~1 可调，透明时边框也全透明

**测试验证：**  
- [ ] 设置对话框调节代码块透明度，确认效果即时生效且可为 0 透明
- [ ] 重启后透明度配置保留，颜色保持默认黑色

**后续 TODO：**  
- 无  
### [编号占位：例如 010] 2025-11-24 统一任务栏与托盘图标加载

日期：2025-11-24  
状态：已完成  

**变更类型：**  
- 功能新增 / 行为调整  

**涉及阶段：**  
- （未区分阶段）  

**目标：**  
- 主窗口任务栏和系统托盘共用同一图标文件，路径清晰且有容错

**触发原因：**  
- 需要统一设置任务栏和托盘图标，使用已有的 app.ico

**涉及文件：**  
- src/app/MainWindow.cpp  
- ai/STAGE_PLAN.md  

**改动概览：**  
- 新增 locateAppIconPath()：在 exe 向上最多 8 级查找 `resources/icons/app.ico`
- 构造函数：若找到有效图标即 setWindowIcon，确保任务栏图标生效
- createSystemTray：优先复用 windowIcon，不存在时再尝试 app.ico，最后退回标准图标

**关键点说明：**  
- 图标放置约定：`resources/icons/app.ico`（相对应用目录，可包含多尺寸）
- 加载失败有兜底（使用系统标准文件图标），不会影响运行

**测试验证：**  
- [ ] 将 app.ico 放入 resources/icons/，启动后任务栏与托盘图标均显示该图标
- [ ] 临时移走 app.ico，启动后托盘仍显示兜底标准图标，程序不崩溃

**后续 TODO：**  
- 无  
### [编号占位：例如 011] 2025-11-24 支持拖拽/打开 TXT

日期：2025-11-24  
状态：已完成  

**变更类型：**  
- 功能新增 / 行为调整  

**涉及阶段：**  
- （未区分阶段）  

**目标：**  
- 让 `.txt` 文件与 Markdown 一样可被打开/拖拽/内部链接跳转

**触发原因：**  
- 用户需求：支持 TXT，预期改动行数较少

**涉及文件：**  
- src/app/MarkdownPage.cpp  
- src/app/MainWindow.cpp  
- ai/STAGE_PLAN.md  

**改动概览：**  
- MarkdownPage：内部链接识别增加 `.txt`
- MainWindow：拖拽判定、内部链接处理、文件对话框过滤、放行扩展名均支持 `.txt`

**关键点说明：**  
- `.txt` 与 `.md/.markdown` 同路径处理，不区分渲染管线

**测试验证：**  
- [ ] 拖拽 `.txt` 打开成功
- [ ] 文件对话框选择 `.txt` 能正常打开
- [ ] 文内链接指向 `.txt` 能跳转

**后续 TODO：**  
- 无  
