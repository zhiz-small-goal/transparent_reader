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
