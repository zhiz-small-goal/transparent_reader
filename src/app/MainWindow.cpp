#include "MainWindow.h"

#include <QCoreApplication>
#include <QDir>
#include <QMouseEvent>
#include <QPoint>
#include <QString>
#include <QUrl>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWebEngineView>
#include <QToolButton>
#include <QLabel>
#include <QWidget>

// ================= 工具函数：找 index.html & 占位 HTML =================
namespace
{
QUrl locateIndexPage()
{
    QDir dir(QCoreApplication::applicationDirPath());
    constexpr int kMaxLevels = 8;
    for (int i = 0; i < kMaxLevels; ++i) {
        if (dir.exists("resources/web/index.html")) {
            return QUrl::fromLocalFile(
                dir.absoluteFilePath("resources/web/index.html"));
        }
        if (!dir.cdUp()) {
            break;
        }
    }
    return {};
}

QString placeholderHtml()
{
    return QStringLiteral(
        R"(<!doctype html>
<html>
  <head>
    <meta charset="utf-8" />
    <title>TransparentMdReader</title>
  </head>
  <body>
    <h1>Hello, TransparentMdReader</h1>
    <p>TODO: hook up Markdown rendering / web channel / state persistence.</p>
  </body>
</html>)");
}

// ================= 自定义标题栏（可拖动 + 按钮） =================
class TitleBar : public QWidget
{
public:
    explicit TitleBar(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        // 和整体窗口保持同样透明度：背景透明，只画一条底部边线
        setAttribute(Qt::WA_StyledBackground, true);
        setStyleSheet(
            // 标题栏本身也是一块很浅的半透明白色
            "background-color: rgba(255, 255, 255, 40);"
            // 和下面内容之间一条若有若无的分割线
            "border-bottom: 1px solid rgba(255, 255, 255, 80);"
        );

        setFixedHeight(32); // 标题栏高度，你可以之后再调

        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(8, 4, 8, 4);
        layout->setSpacing(6);

        // 左侧标题文字（后面可以改成图标 + 名称）
        auto *titleLabel = new QLabel(QStringLiteral("TransparentMdReader"), this);
        titleLabel->setStyleSheet("color: white;");
        layout->addWidget(titleLabel);
        layout->addStretch(1);

        // 右侧按钮区：-  🔒  ⚙  ×
        auto makeButton = [this](const QString &text, const QString &tooltip) {
            auto *btn = new QToolButton(this);
            btn->setText(text);
            btn->setToolTip(tooltip);
            btn->setAutoRaise(true);
            btn->setCursor(Qt::PointingHandCursor);
            btn->setStyleSheet(
                "QToolButton {"
                "  color: white;"
                "  background-color: transparent;"
                "  padding: 2px 6px;"
                "}"
                "QToolButton:hover {"
                "  background-color: rgba(255, 255, 255, 30);"
                "}"
            );
            return btn;
        };

        auto *minBtn  = makeButton(QStringLiteral("−"), QStringLiteral("最小化"));
        auto *lockBtn = makeButton(QStringLiteral("🔒"), QStringLiteral("锁定（预留）"));
        auto *cfgBtn  = makeButton(QStringLiteral("⚙"), QStringLiteral("设置（预留）"));
        auto *closeBtn= makeButton(QStringLiteral("×"), QStringLiteral("关闭"));

        layout->addWidget(minBtn);
        layout->addWidget(lockBtn);
        layout->addWidget(cfgBtn);
        layout->addWidget(closeBtn);

        // 按钮行为：直接操作窗口
        connect(minBtn, &QToolButton::clicked, this, [this]() {
            if (QWidget *win = window()) {
                win->showMinimized();
            }
        });

        // 先把 🔒 / ⚙ 预留出来，未来可以在 MainWindow 里加接口来控制
        connect(lockBtn, &QToolButton::clicked, this, []() {
            // TODO: 这里以后加「锁定」功能（例如取消拖动 / 锁定透明度等）
        });

        connect(cfgBtn, &QToolButton::clicked, this, []() {
            // TODO: 这里以后打开设置界面 / 配置对话框
        });

        connect(closeBtn, &QToolButton::clicked, this, [this]() {
            if (QWidget *win = window()) {
                win->close();
            }
        });
    }

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton) {
            m_dragging = true;
            if (QWidget *win = window()) {
                m_dragOffset =
                    event->globalPosition().toPoint()
                    - win->frameGeometry().topLeft();
            }
            event->accept();
        } else {
            QWidget::mousePressEvent(event);
        }
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (m_dragging) {
            if (QWidget *win = window()) {
                const QPoint globalPos = event->globalPosition().toPoint();
                win->move(globalPos - m_dragOffset);
            }
            event->accept();
        } else {
            QWidget::mouseMoveEvent(event);
        }
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && m_dragging) {
            m_dragging = false;
            event->accept();
        } else {
            QWidget::mouseReleaseEvent(event);
        }
    }

private:
    bool  m_dragging   = false;
    QPoint m_dragOffset;
};

} // namespace

// ================= MainWindow 实现 =================

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // 无边框 + 置顶 + 透明
    setWindowFlags(windowFlags()
                   | Qt::FramelessWindowHint
                   | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowOpacity(0.92);

    resize(720, 900);
    setMinimumSize(480, 600);

    // 中央容器：上面标题栏，下面 QWebEngineView
    auto *central = new QWidget(this);
    central->setAttribute(Qt::WA_TranslucentBackground);
    central->setAutoFillBackground(false);

    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // 顶部标题栏（可拖动）
    auto *titleBar = new TitleBar(central);
    layout->addWidget(titleBar);

    // WebEngine 区域
    m_view = new QWebEngineView(central);
    layout->addWidget(m_view, 1);

    setCentralWidget(central);

    // 加载本地 index.html 或占位 HTML
    const QUrl pageUrl = locateIndexPage();
    if (pageUrl.isValid()) {
        m_view->load(pageUrl);
    } else {
        m_view->setHtml(placeholderHtml());
    }
}

MainWindow::~MainWindow() = default;
