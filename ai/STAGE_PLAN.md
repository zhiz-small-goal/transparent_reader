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
