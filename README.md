# TransparentMdReader

透明的 Markdown 阅读器，基于 C++17 + Qt6 (Widgets / WebEngine / WebChannel)，支持锁定/点穿、右键翻页、Ctrl 临时解锁，并记住阅读进度。

## 功能特性
- 渲染本地 Markdown（前端使用 markdown-it）
- 透明窗口、可置顶、无边框，自定义窗口大小（含最小值限制）
- 阅读样式可调：字号、字体颜色/透明度、背景透明度、代码块透明度、滚动条显示
- 记忆每个文档的滚动位置与最近打开列表
- 系统托盘：显示/隐藏、最近列表、开机自启开关、日志开关
- 单实例，拖拽或文件对话框打开 `.md`/`.markdown`
- 图片链接浮层，内部 `.md` 链接跳转

## 运行环境
- Windows 10/11
- C++17，Qt 6.x（需 WebEngine 模块）
- CMake 3.20+，可用 Visual Studio / Qt Creator / VS Code

## 目录结构（简要）
```
resources/web/      # index.html / main.js / style.css / markdown-it.min.js
resources/icons/    # app.ico（任务栏与托盘共用）
src/app/            # MainWindow、MarkdownPage、TitleBar 等
docs/               # 设计文档、状态库设计
ai/                 # AI 相关日志/计划
```

## 快速构建与运行
1) 准备依赖：安装 Qt 6.x（含 WebEngine），配置好 CMake 和编译器。
2) 生成工程（示例 VS 2022 x64）：
```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
```
3) 编译：
```
cmake --build build --config Release
```
4) 运行：执行生成的 `TransparentMdReader.exe`。
5) 确认资源：`resources/web/` 下的 HTML/JS/CSS/markdown-it.min.js 存在；图标放 `resources/icons/app.ico`。

## 使用说明
- 打开文件：Ctrl+O 或拖拽 `.md`；托盘菜单也可打开。
- 锁定/解锁：标题栏锁按钮；按住 Ctrl 临时解锁；右键翻页受锁定控制。
- 阅读进度：自动记录每个文件的滚动位置，重开恢复。
- 设置：标题栏齿轮——调字号/颜色/透明度、代码块透明度、滚动条、窗口大小、历史/最近上限。
- 托盘：单/双击托盘图标切换显/隐；最近列表可直接打开；可切换开机自启与日志。

## 配置与存储
- QSettings：窗口尺寸、阅读样式、历史/最近上限等。
- 状态数据库（SQLite）：阅读进度、最近文件等。
- 路径使用相对路径，资源默认随应用目录部署。

## 开发提示
- Qt 逻辑集中在 `src/app/`；前端在 `resources/web/`。
- 样式注入在 `MainWindow::applyReaderStyle()`；链接/图片拦截在 `MarkdownPage`。
- 修改行为前可先参考 `docs/transparent_reader_dev_doc.md`、`docs/transparentmdreader_state_db_design.md` 等。

## 许可证
（请在此处补充实际许可证，如 MIT/Apache-2.0 等）
