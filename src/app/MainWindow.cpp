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
#include <QMessageBox>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QShortcut>

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

// 一个非常简单的 Markdown → HTML，占个位，后面会被正式渲染管线替换
QString basicMarkdownToHtml(const QString &markdown, const QString &title)
{
    // 先把 <, >, & 转义，避免当成 HTML 标签
    QString escaped = markdown.toHtmlEscaped();
    // 保留换行
    escaped.replace("\n", "<br/>\n");

    const QString pageTitle =
        title.isEmpty() ? QStringLiteral("TransparentMdReader") : title;

    const QString html = QStringLiteral(
        "<!doctype html>\n"
        "<html>\n"
        "<head>\n"
        "  <meta charset=\"utf-8\" />\n"
        "  <title>%1</title>\n"
        "  <style>\n"
        "    body {\n"
        "      margin: 24px;\n"
        "      color: #f5f5f5;\n"
        "      background-color: rgba(45, 44, 44, 0.55);\n"
        "      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;\n"
        "      line-height: 1.6;\n"
        "    }\n"
        "    .md-content {\n"
        "      white-space: pre-wrap;\n"
        "    }\n"
        "  </style>\n"
        "</head>\n"
        "<body>\n"
        "  <div class=\"md-content\">\n"
        "%2\n"
        "  </div>\n"
        "</body>\n"
        "</html>\n")
            .arg(pageTitle, escaped);

    return html;
}

// ================= 自定义标题栏（可拖动 + 按钮） =================
class TitleBar : public QWidget
{
public:
    explicit TitleBar(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        // 标题栏：和整体保持类似的浅色感 + 一条若有若无的分割线
        setAttribute(Qt::WA_StyledBackground, true);
        setStyleSheet(
            "background-color: rgba(255, 255, 255, 40);"
            "border-bottom: 1px solid rgba(255, 255, 255, 80);"
        );

        setFixedHeight(32);

        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(8, 4, 8, 4);
        layout->setSpacing(6);

        // 左侧标题文字
        auto *titleLabel =
            new QLabel(QStringLiteral("TransparentMdReader"), this);
        titleLabel->setStyleSheet("color: white;");
        layout->addWidget(titleLabel);
        layout->addStretch(1);

        // 右侧按钮区：−  🔒/🔓  ⚙  ×
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

        auto *minBtn   = makeButton(QStringLiteral("−"),
                                    QStringLiteral("最小化"));
        // 初始为“未锁定”状态，用 🔓，提示点击后会锁定
        m_lockBtn      = makeButton(QStringLiteral("🔓"),
                                    QStringLiteral("点击锁定窗口（禁止拖动）"));
        auto *cfgBtn   = makeButton(QStringLiteral("⚙"),
                                    QStringLiteral("设置"));
        auto *closeBtn = makeButton(QStringLiteral("×"),
                                    QStringLiteral("关闭"));

        layout->addWidget(minBtn);
        layout->addWidget(m_lockBtn);
        layout->addWidget(cfgBtn);
        layout->addWidget(closeBtn);

        // 按钮行为：直接操作窗口
        connect(minBtn, &QToolButton::clicked, this, [this]() {
            if (QWidget *win = window()) {
                win->showMinimized();
            }
        });

        // 锁定按钮：只控制是否允许拖动，并更新图标 / 提示
        connect(m_lockBtn, &QToolButton::clicked, this, [this]() {
            m_locked = !m_locked;
            if (m_locked) {
                m_lockBtn->setText(QStringLiteral("🔒"));
                m_lockBtn->setToolTip(
                    QStringLiteral("已锁定：点击解锁窗口（允许拖动）"));
            } else {
                m_lockBtn->setText(QStringLiteral("🔓"));
                m_lockBtn->setToolTip(
                    QStringLiteral("已解锁：点击锁定窗口（禁止拖动）"));
            }
        });

        // 设置按钮：先弹一个占位的设置对话框，后面再接真正设置界面
        connect(cfgBtn, &QToolButton::clicked, this, [this]() {
            QMessageBox::information(
                window(),
                QStringLiteral("设置"),
                QStringLiteral(
                    "设置界面尚未实现。\n\n"
                    "后续会在这里添加 TransparentMdReader 的配置选项。"));
        });

        // 关闭按钮：关闭窗口
        connect(closeBtn, &QToolButton::clicked, this, [this]() {
            if (QWidget *win = window()) {
                win->close();
            }
        });
    }

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && !m_locked) {
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
        if (m_dragging && !m_locked) {
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
    bool         m_dragging   = false;
    bool         m_locked     = false;
    QPoint       m_dragOffset;
    QToolButton *m_lockBtn    = nullptr;
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

    // 快捷键：Ctrl+O 打开本地 Markdown 文件
    auto *openShortcut = new QShortcut(QKeySequence::Open, this);
    connect(openShortcut, &QShortcut::activated,
            this, &MainWindow::openMarkdownFileFromDialog);
}

MainWindow::~MainWindow() = default;

// 打开文件对话框，选择 .md 再调用 openMarkdownFile()
void MainWindow::openMarkdownFileFromDialog()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("打开 Markdown 文件"),
        QString(),
        QStringLiteral("Markdown 文件 (*.md *.markdown);;所有文件 (*.*)")
    );

    if (path.isEmpty()) {
        return;
    }

    openMarkdownFile(path);
}

// 读取指定 .md 并在 WebEngine 中显示
void MainWindow::openMarkdownFile(const QString &path)
{
    if (!m_view) {
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(
            this,
            QStringLiteral("打开失败"),
            QStringLiteral("无法打开文件：\n%1").arg(path));
        return;
    }

    QTextStream in(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    in.setCodec("UTF-8");
#else
    in.setEncoding(QStringConverter::Utf8);
#endif
    const QString markdown = in.readAll();
    file.close();

    const QString fileName = QFileInfo(path).fileName();
    const QString html = basicMarkdownToHtml(markdown, fileName);

    // 第二个参数给 baseUrl，方便后面相对链接（图片等）生效
    m_view->setHtml(html, QUrl::fromLocalFile(path));

    // 同步一下窗口标题，方便区分当前打开哪个文件
    setWindowTitle(QStringLiteral("TransparentMdReader - %1").arg(fileName));
}
