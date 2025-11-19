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
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
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
#include <QDesktopServices>
#include <QTimer>



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

// ================= 图片查看浮层（半透明背景 + 右上角关闭） =================


} // namespace

class ImageOverlay : public QWidget      // NEW
{
public:
    explicit ImageOverlay(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_StyledBackground, true);
        setStyleSheet("background-color: rgba(0, 0, 0, 180);");
        setMouseTracking(true);
        setVisible(false);

        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setAlignment(Qt::AlignCenter);

        m_imageLabel = new QLabel(this);
        m_imageLabel->setAlignment(Qt::AlignCenter);
        m_imageLabel->setSizePolicy(QSizePolicy::Ignored,
                                    QSizePolicy::Ignored);
        m_imageLabel->setScaledContents(true);
        layout->addWidget(m_imageLabel);

        m_closeBtn = new QToolButton(this);
        m_closeBtn->setText(QStringLiteral("×"));
        m_closeBtn->setToolTip(QStringLiteral("关闭图片"));
        m_closeBtn->setCursor(Qt::PointingHandCursor);
        m_closeBtn->setAutoRaise(true);
        m_closeBtn->setStyleSheet(
            "QToolButton {"
            "  color: white;"
            "  background-color: transparent;"
            "  padding: 2px 6px;"
            "  border-radius: 12px;"
            "}"
            "QToolButton:hover {"
            "  background-color: rgba(255, 255, 255, 40);"
            "}"
        );
        m_closeBtn->hide();

        connect(m_closeBtn, &QToolButton::clicked,
                this, &ImageOverlay::hide);
    }

    bool showImage(const QString &filePath)
    {
        QPixmap pix(filePath);
        if (pix.isNull()) {
            return false;
        }

        m_imageLabel->setPixmap(pix);

        if (QWidget *p = parentWidget()) {
            resize(p->size());
            move(0, 0);
        }

        show();
        raise();
        m_closeBtn->show();
        return true;
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QWidget::resizeEvent(event);
        if (m_closeBtn) {
            const int margin = 12;
            m_closeBtn->move(width() - m_closeBtn->width() - margin, margin);
        }
    }

    void enterEvent(QEnterEvent *event) override
    {
        Q_UNUSED(event);
        if (m_closeBtn) {
            m_closeBtn->show();    // 鼠标移入浮层时显示关闭按钮
        }
    }

    void leaveEvent(QEvent *event) override
    {
        QWidget::leaveEvent(event);
        if (m_closeBtn) {
            m_closeBtn->hide();    // 鼠标移出浮层时隐藏关闭按钮
        }
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        // 点击图片之外：关闭浮层
        if (m_imageLabel
            && !m_imageLabel->geometry().contains(event->pos())) {
            hide();
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }

private:
    QLabel      *m_imageLabel = nullptr;
    QToolButton *m_closeBtn   = nullptr;
};

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
    setAcceptDrops(true);

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
    m_view->setAcceptDrops(false);   // 由 MainWindow 统一处理拖拽打开
    layout->addWidget(m_view, 1);

    // 使用自定义 QWebEnginePage（MarkdownPage）拦截链接点击
    auto *page = new MarkdownPage(m_view);
    m_view->setPage(page);
    connect(page, &MarkdownPage::openMarkdown,
            this, &MainWindow::handleOpenMarkdownUrl);

    connect(page, &MarkdownPage::openImage,              
        this, &MainWindow::handleOpenImageUrl);      


    setCentralWidget(central);

    // 初始化“最后打开目录”：优先用文档目录，其次 home 目录，然后看配置
    QSettings settings("zhiz", "TransparentMdReader");
    const QString savedDir  = settings.value("ui/lastOpenDir").toString();
    const QString savedFile = settings.value("ui/lastFilePath").toString();

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

    QString startupFilePath;
    if (!savedFile.isEmpty()) {
        QFileInfo savedFileInfo(savedFile);
        if (savedFileInfo.exists() && savedFileInfo.isFile()) {
            startupFilePath = savedFileInfo.absoluteFilePath();
        }
    }

    // 文件：src/app/MainWindow.cpp （构造函数内部）

    const QUrl pageUrl = locateIndexPage();
    if (pageUrl.isValid()) {
        m_useEmbeddedViewer = true;
        m_view->load(pageUrl);
        connect(m_view, &QWebEngineView::loadFinished,
                this, [this](bool ok) {
                    m_pageLoaded = ok;
                    if (ok && !m_pendingMarkdown.isEmpty()) {
                        const QUrl baseUrl(m_pendingBaseUrl);
                        renderMarkdownInPage(m_pendingMarkdown,
                                             m_pendingTitle,
                                             baseUrl);
                        m_pendingMarkdown.clear();
                        m_pendingTitle.clear();
                        m_pendingBaseUrl.clear();
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

    if (!startupFilePath.isEmpty()) {
        QTimer::singleShot(0, this, [this, startupFilePath]() {
            openMarkdownFile(startupFilePath);
        });
    }
}

MainWindow::~MainWindow() = default;

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (!event) {
        return;
    }

    const QMimeData *mimeData = event->mimeData();
    if (!mimeData || !mimeData->hasUrls()) {
        QMainWindow::dragEnterEvent(event);
        return;
    }

    const auto urls = mimeData->urls();
    for (const QUrl &url : urls) {
        if (!url.isLocalFile()) {
            continue;
        }

        const QString filePath = url.toLocalFile();
        if (filePath.isEmpty()) {
            continue;
        }

        const QString lower = filePath.toLower();
        if (lower.endsWith(".md") || lower.endsWith(".markdown")) {
            event->acceptProposedAction();
            return;
        }
    }

    QMainWindow::dragEnterEvent(event);
}

void MainWindow::dropEvent(QDropEvent *event)
{
    if (!event) {
        return;
    }

    const QMimeData *mimeData = event->mimeData();
    if (!mimeData || !mimeData->hasUrls()) {
        QMainWindow::dropEvent(event);
        return;
    }

    const auto urls = mimeData->urls();
    for (const QUrl &url : urls) {
        if (!url.isLocalFile()) {
            continue;
        }

        const QString filePath = url.toLocalFile();
        if (filePath.isEmpty()) {
            continue;
        }

        const QString lower = filePath.toLower();
        if (!lower.endsWith(".md") && !lower.endsWith(".markdown")) {
            continue;
        }

        event->acceptProposedAction();
        openMarkdownFile(filePath);
        return;
    }

    QMainWindow::dropEvent(event);
}

// 文件：src/app/MainWindow.cpp
// 说明：处理文内 .md 链接的点击（由 MarkdownPage::openMarkdown 信号触发）
void MainWindow::handleOpenMarkdownUrl(const QUrl &url)  // CHANGED
{
    // 还没有成功打开过任何 md，没法做内部跳转
    if (m_currentFilePath.isEmpty()) {
        return;
    }

    // 从 URL 中提取“文件名”，不关心前面是 file:/// 还是 qrc:/ 等
    const QString urlPath  = url.path();                   // 例如 /F:/.../4-introduction....md
    const QString fileName = QFileInfo(urlPath).fileName();// 例如 4-introduction....md
    const QString lower    = fileName.toLower();

    // 只处理 .md / .markdown，其它一律忽略（http/https 已在 MarkdownPage 里处理）
    if (!lower.endsWith(".md") && !lower.endsWith(".markdown")) {
        return;
    }

    // 目标路径 = 当前 md 所在目录 + 链接里的文件名
    QFileInfo curFi(m_currentFilePath);
    QDir      baseDir(curFi.absolutePath());
    const QString targetPath = baseDir.absoluteFilePath(fileName);

    // if (targetPath.isEmpty()) {
    //     return;
    // }

    QFileInfo fi(targetPath);
    if (!fi.exists() || !fi.isFile()) {
        QMessageBox::warning(
            this,
            QStringLiteral("打开失败"),
            QStringLiteral("找不到链接指向的 Markdown 文件：\n%1")
                .arg(targetPath));
        return;
    }

    // 统一走 openMarkdownFile：记忆目录 + 当前文件路径 + 渲染全部在里面
    openMarkdownFile(fi.absoluteFilePath());
}


// 文件：src/app/MainWindow.cpp
// 处理图片链接点击：在阅读器内弹出图片浮层
void MainWindow::handleOpenImageUrl(const QUrl &url)
{
    QString localPath;

    if (url.isLocalFile()) {
        localPath = url.toLocalFile();
    } else if (url.scheme().isEmpty()
               || url.scheme() == QStringLiteral("file")) {
        // 相对路径：基于当前 md 所在目录解析
        if (m_currentFilePath.isEmpty()) {
            return;
        }
        QFileInfo curFi(m_currentFilePath);
        QDir      baseDir(curFi.absolutePath());
        localPath = baseDir.absoluteFilePath(url.path());
    } else {
        // http/https 等仍然交给系统浏览器
        QDesktopServices::openUrl(url);
        return;
    }

    if (localPath.isEmpty()) {
        return;
    }

    QFileInfo fi(localPath);
    if (!fi.exists() || !fi.isFile()) {
        QMessageBox::warning(
            this,
            QStringLiteral("打开失败"),
            QStringLiteral("找不到图片文件：\n%1").arg(localPath));
        return;
    }

    if (!m_imageOverlay) {
        m_imageOverlay = new ImageOverlay(this);
    }

    if (!m_imageOverlay->showImage(fi.absoluteFilePath())) {
        QMessageBox::warning(
            this,
            QStringLiteral("打开失败"),
            QStringLiteral("无法加载图片：\n%1").arg(localPath));
    }
}


// 文件：src/app/MainWindow.cpp
// 函数：MainWindow::renderMarkdownInPage  （请直接整体替换）

void MainWindow::renderMarkdownInPage(const QString &markdown,
                                      const QString &title,
                                      const QUrl    &baseUrl)
{
    if (!m_view) return;

    QString js = QStringLiteral(
        "window.renderMarkdown(%1, %2, %3);"
    )
                     .arg(toJsStringLiteral(markdown))
                     .arg(toJsStringLiteral(title))
                     .arg(toJsStringLiteral(baseUrl.toString()));

    m_view->page()->runJavaScript(js);
}


// 文件：src/app/MainWindow.cpp
// 作用：弹出文件对话框选择 .md，并调用 openMarkdownFile 打开
void MainWindow::openMarkdownFileFromDialog()   // NEW
{
    // 起始目录：优先用记忆目录，其次文档目录，最后用户 home
    QString startDir = m_lastOpenDir;
    if (startDir.isEmpty()) {
        startDir =
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        if (startDir.isEmpty()) {
            startDir = QDir::homePath();
        }
    }

    const QString filter =
        QStringLiteral("Markdown 文件 (*.md *.markdown);;所有文件 (*.*)");

    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("打开 Markdown 文件"),
        startDir,
        filter);

    if (path.isEmpty()) {
        return;
    }

    openMarkdownFile(path);
}


// 文件：src/app/MainWindow.cpp
// 读取指定 .md 并渲染显示（当前仍使用 basicMarkdownToHtml 占位渲染）
void MainWindow::openMarkdownFile(const QString &path)
{
    if (!m_view) return;

    QFileInfo fi(path);
    if (!fi.exists() || !fi.isFile()) {
        QMessageBox::warning(
            this,
            QStringLiteral("打开失败"),
            QStringLiteral("找不到文件：\n%1").arg(path));
        return;
    }

    // 记录路径
    m_lastOpenDir     = fi.absolutePath();
    m_currentFilePath = fi.absoluteFilePath();

    // 写入设置
    {
        QSettings settings("zhiz", "TransparentMdReader");
        settings.setValue("ui/lastOpenDir", m_lastOpenDir);
        settings.setValue("ui/lastFilePath", m_currentFilePath);
    }

    // 读取 Markdown 文本
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
    QString markdown = in.readAll();
    file.close();

    // 准备传给前端的 baseUrl
    QUrl baseUrl = QUrl::fromLocalFile(fi.absolutePath() + "/");

    // 加载前端 index.html（如果尚未加载）
    if (!m_pageLoaded) {
        const QUrl index = locateIndexPage();

        m_pendingMarkdown = markdown;
        m_pendingTitle    = fi.fileName();
        m_pendingBaseUrl  = baseUrl.toString();

        m_view->setUrl(index);
        return;
    }

    // 前端已就绪：直接渲染
    renderMarkdownInPage(markdown, fi.fileName(), baseUrl);
    setWindowTitle(QStringLiteral("TransparentMdReader - %1").arg(fi.fileName()));
}
