#include "MainWindow.h"
#include "MarkdownPage.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPoint>
#include <QShortcut>
#include <QStandardPaths>
#include <QSettings>
#include <QTextStream>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QWebEngineView>
#include <QWidget>
#include <QRegularExpression>


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
    <h1>TransparentMdReader</h1>
    <p>未找到前端资源（resources/web/index.html）。</p>
    <p>请检查工程中的 resources/web/ 目录。</p>
  </body>
</html>)");
}

/// 一个非常简单的 Markdown → HTML，占个位，后面会被正式渲染管线替换
QString basicMarkdownToHtml(const QString &markdown, const QString &title)
{
    // 先把 <, >, & 等转义，防止破坏 HTML 结构
    QString escaped = markdown.toHtmlEscaped();

    // 1) 把 [文本](链接) 变成 <a href="链接">文本</a>
    static const QRegularExpression linkRe(
        R"(\[([^\]]+)\]\(([^)]+)\))");
    escaped.replace(linkRe, R"(<a href="\2">\1</a>)");

    // 2) 保留换行
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
        "    a {\n"
        "      color: rgba(80, 160, 255, 0.95);\n"
        "      text-decoration: underline;\n"
        "      cursor: pointer;\n"
        "      transition: opacity 0.12s ease;\n"
        "    }\n"
        "    a:hover {\n"
        "      opacity: 0.8;\n"
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


// 把 QString 编码成 JS 字符串字面量：'...'
QString toJsStringLiteral(const QString &str)
{
    QString s = str;
    s.replace("\\", "\\\\");
    s.replace("'", "\\'");
    s.replace("\r", "");
    s.replace("\n", "\\n");
    s.replace("\t", "\\t");
    return "'" + s + "'";
}

// ================= 自定义标题栏（可拖动 + 按钮） =================
class TitleBar : public QWidget
{
public:
    explicit TitleBar(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_StyledBackground, true);
        setStyleSheet(
            "background-color: rgba(255, 255, 255, 40);"
            "border-bottom: 1px solid rgba(255, 255, 255, 80);"
        );

        setFixedHeight(32);

        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(8, 4, 8, 4);
        layout->setSpacing(6);

        auto *titleLabel =
            new QLabel(QStringLiteral("TransparentMdReader"), this);
        titleLabel->setStyleSheet("color: white;");
        layout->addWidget(titleLabel);
        layout->addStretch(1);

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

        connect(minBtn, &QToolButton::clicked, this, [this]() {
            if (QWidget *win = window()) {
                win->showMinimized();
            }
        });

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

        connect(cfgBtn, &QToolButton::clicked, this, [this]() {
            QMessageBox::information(
                window(),
                QStringLiteral("设置"),
                QStringLiteral(
                    "设置界面尚未实现。\n\n"
                    "后续会在这里添加 TransparentMdReader 的配置选项。"));
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

    auto *titleBar = new TitleBar(central);
    layout->addWidget(titleBar);

    // WebEngine 区域
    m_view = new QWebEngineView(central);
    auto *page = new MarkdownPage(m_view);      // ✅ 使用自定义 QWebEnginePage
    m_view->setPage(page);
    layout->addWidget(m_view, 1);

    // 连接内部 Markdown 链接信号
    connect(page, &MarkdownPage::openMarkdown,
            this, &MainWindow::handleOpenMarkdownUrl);


    setCentralWidget(central);

    // 初始化“最后打开目录”：优先用文档目录，其次 home 目录，然后看配置
    QSettings settings("zhiz", "TransparentMdReader");
    const QString savedDir = settings.value("ui/lastOpenDir").toString();

    QString defaultDir =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (defaultDir.isEmpty()) {
        defaultDir = QDir::homePath();
    }

    if (!savedDir.isEmpty() && QDir(savedDir).exists()) {
        m_lastOpenDir = savedDir;
    } else {
        m_lastOpenDir = defaultDir;
    }

    // 加载前端页面（如有），否则用占位 HTML
    const QUrl pageUrl = locateIndexPage();
    if (pageUrl.isValid()) {
        m_useEmbeddedViewer = true;
        m_view->load(pageUrl);
        connect(m_view, &QWebEngineView::loadFinished,
                this, [this](bool ok) {
                    m_pageLoaded = ok;
                    if (ok && !m_pendingMarkdown.isEmpty()) {
                        renderMarkdownInPage(m_pendingMarkdown, m_pendingTitle);
                        m_pendingMarkdown.clear();
                        m_pendingTitle.clear();
                    }
                });
    } else {
        m_useEmbeddedViewer = false;
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
    const QString startDir =
        m_lastOpenDir.isEmpty() ? QDir::homePath() : m_lastOpenDir;

    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("打开 Markdown 文件"),
        startDir,
        QStringLiteral("Markdown 文件 (*.md *.markdown);;所有文件 (*.*)")
    );

    if (path.isEmpty()) {
        return;
    }

    openMarkdownFile(path);
}
void MainWindow::handleOpenMarkdownUrl(const QUrl &url)
{
    // 如果当前还没有打开任何 md，就没法解析相对路径
    if (m_currentFilePath.isEmpty()) {
        return;
    }

    QFileInfo currentFi(m_currentFilePath);
    QDir      baseDir(currentFi.absolutePath());

    QString localPath;

    if (url.isRelative()) {
        // 相对路径：基于当前 md 文件所在目录
        localPath = baseDir.absoluteFilePath(url.path());
    } else if (url.isLocalFile()) {
        // file:// 本地路径
        localPath = url.toLocalFile();
    } else {
        // 正常不会走到这里：外部链接已经在 MarkdownPage 里交给系统浏览器了
        return;
    }

    if (localPath.isEmpty()) {
        return;
    }

    QFileInfo fi(localPath);
    if (!fi.exists()) {
        QMessageBox::warning(
            this,
            QStringLiteral("打开失败"),
            QStringLiteral("找不到链接指向的 Markdown 文件：\n%1")
                .arg(localPath));
        return;
    }

    openMarkdownFile(fi.absoluteFilePath());
}



// 调用前端 JS 进行渲染
void MainWindow::renderMarkdownInPage(const QString &markdown,
                                      const QString &title)
{
    if (!m_view) {
        return;
    }

    const QString js =
        QStringLiteral("window.renderMarkdown(%1, %2);")
            .arg(toJsStringLiteral(markdown), toJsStringLiteral(title));

    m_view->page()->runJavaScript(js);
}

// 读取指定 .md 并渲染显示
void MainWindow::openMarkdownFile(const QString &path)
{
    if (!m_view) {
        return;
    }

    // 记录当前文件和目录
    QFileInfo fi(path);
    m_lastOpenDir     = fi.absolutePath();
    m_currentFilePath = fi.absoluteFilePath();

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

    const QString fileName = fi.fileName();
    const QString html     = basicMarkdownToHtml(markdown, fileName);

    // 第二个参数给 baseUrl，方便后面相对链接（图片 / 内部链接）生效
    m_view->setHtml(html, QUrl::fromLocalFile(path));

    // 窗口标题也带上文件名
    setWindowTitle(QStringLiteral("TransparentMdReader - %1").arg(fileName));
}

