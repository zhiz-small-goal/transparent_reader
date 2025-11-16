# 阶段计划

---

## 阶段 0：工程骨架（最小可运行窗口）

日期：2025-11-16
状态：进行中

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
状态：进行中

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
状态：进行中

目标：
- 在无边框窗口顶部添加自定义标题栏，整体透明度与窗口一致。
- 标题栏与内容区域之间添加一条若有若无的分割线（浅色半透明）。
- 在标题栏右侧添加常规按钮：最小化（−）、锁定（🔒）、设置（⚙）、关闭（×）。
- 标题栏空白区域支持按住左键拖动整个窗口。

涉及文件：
- D:/zhiz-c++/transparent_reader/src/app/MainWindow.h
- D:/zhiz-c++/transparent_reader/src/app/MainWindow.cpp

