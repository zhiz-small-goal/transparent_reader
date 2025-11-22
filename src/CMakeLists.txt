# D:/zhiz-c++/transparent_reader/src/CMakeLists.txt

# 1. 找到 Qt 6 需要的模块
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    WebEngineWidgets
    WebChannel
    Sql
)

# 2. 启用 Qt 标准项目设置（里面会自动开 AUTOMOC / AUTOUIC / AUTORCC）
qt_standard_project_setup()

# 3. 定义可执行程序
# 如果你的 main.cpp / MainWindow.cpp / .h 就在 src 目录，用这一段：
# qt_add_executable(TransparentMdReader
#     MANUAL_FINALIZATION

#     main.cpp
#     MainWindow.cpp
#     MainWindow.h
# )

# 如果你的文件在 src/app/ 下面，请改成下面这样（注意注释）：
qt_add_executable(TransparentMdReader
    MANUAL_FINALIZATION

    app/main.cpp
    app/MainWindow.cpp
    app/MainWindow.h
    app/MarkdownPage.cpp
    app/MarkdownPage.h
    app/StateDbManager.cpp
    app/StateDbManager.h
)

# 4. 链接 Qt 库
target_link_libraries(TransparentMdReader PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::WebEngineWidgets
    Qt6::WebChannel
    Qt6::Sql
)

# 5. Windows 上去掉控制台黑框
set_target_properties(TransparentMdReader PROPERTIES
    WIN32_EXECUTABLE TRUE
)

# 6. Qt 6 要求：最终收尾
qt_finalize_executable(TransparentMdReader)
