#include "MainWindow.h"
#include "MarkdownPage.h"
#include "StateDbManager.h"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPoint>
#include <QRegularExpression>
#include <QSettings>
#include <QShortcut>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QWebEnginePage>
#include <QWebEngineView>
#include <QWidget>
#include <QDesktopServices>
#include <QEvent>
#include <QColor>
#include <QColorDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QCheckBox>
#include <QSystemTrayIcon>
#include <QStyle>
#include <QSignalBlocker>
#include <QDateTime>
#include <QMutex>
#include <QMutexLocker>
#include <QStringConverter>
#include <QIcon>
#include <cstdio>


#ifdef Q_OS_WIN                    // Windows 原生消息处理
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <windowsx.h>           // GET_X_LPARAM / GET_Y_LPARAM

// 给任意 QWidget 开/关鼠标穿透（只改 WS_EX_TRANSPARENT）
static void setWindowClickThrough(QWidget *w, bool enabled)
{
    if (!w) return;

    HWND hwnd = reinterpret_cast<HWND>(w->winId());
    if (!hwnd) return;

    LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if (enabled) {
        ex |= WS_EX_TRANSPARENT;      // 鼠标命中时当自己不存在
    } else {
        ex &= ~WS_EX_TRANSPARENT;
    }
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, ex);
}
#endif





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
        "      color: rgba(87, 149, 224, 0.95);\n"
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

// ================= 阅读样式 =================

// 阅读样式：字体大小 / 字体颜色 / 背景颜色 / 背景透明度
struct ReaderStyle {
    int   fontPointSize      = 16;                         // 字号（pt）
    QColor fontColor         = QColor(QStringLiteral("#f5f5f5"));  // 接近你占位页的默认颜色
    qreal fontOpacity        = 1.0;                        // 字体透明度 0.0 ~ 1.0
    QColor backgroundColor   = QColor(45, 44, 44);         // 深灰色背景
    qreal  backgroundOpacity = 0.0;                       // 0.0 ~ 1.0
    bool  showScrollbar      = false;                      // 默认隐藏右侧滚动条
};

// 当前全局阅读样式（单窗口应用，用一个全局即可）
ReaderStyle g_readerStyle;

// 把 QColor 转成 CSS rgba(...) 字符串
QString colorToCssRgba(const QColor &c, qreal alphaOverride = -1.0)
{
    QColor tmp = c;
    qreal a = (alphaOverride >= 0.0) ? alphaOverride : tmp.alphaF();
    if (a < 0.0) a = 0.0;
    if (a > 1.0) a = 1.0;

    return QStringLiteral("rgba(%1,%2,%3,%4)")
        .arg(tmp.red())
        .arg(tmp.green())
        .arg(tmp.blue())
        .arg(a, 0, 'f', 3);
}


#ifdef Q_OS_WIN
QString autoStartRegKey()
{
    return QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run");
}

QString autoStartValueName()
{
    return QStringLiteral("TransparentMdReader");
}

bool queryAutoStartEnabled()
{
    QSettings reg(autoStartRegKey(), QSettings::NativeFormat);
    const QString exePath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    const QString current = reg.value(autoStartValueName()).toString();
    return current.compare(exePath, Qt::CaseInsensitive) == 0;
}

bool applyAutoStartEnabled(bool enabled, QString &error)
{
    QSettings reg(autoStartRegKey(), QSettings::NativeFormat);
    const QString exePath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    if (enabled) {
        reg.setValue(autoStartValueName(), exePath);
    } else {
        reg.remove(autoStartValueName());
    }
    reg.sync();
    if (reg.status() != QSettings::NoError) {
        error = QObject::tr("写入注册表失败，请检查权限。");
        return false;
    }
    return true;
}
#else
bool queryAutoStartEnabled()
{
    return false;
}

bool applyAutoStartEnabled(bool /*enabled*/, QString &error)
{
    error = QObject::tr("当前平台暂未实现开机自启开关。");
    return false;
}
#endif

QFile g_logFile;
QtMessageHandler g_prevHandler = nullptr;
bool g_logEnabled = false;
QMutex g_logMutex;

QString logFilePath()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dir.isEmpty()) {
        dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    }
    if (dir.isEmpty()) {
        dir = QDir::tempPath();
    }

    QDir d(dir);
    d.mkpath(QStringLiteral("."));
    return d.filePath(QStringLiteral("transparent_reader.log"));
}

void fileLogHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    if (g_logEnabled && g_logFile.isOpen()) {
        QMutexLocker locker(&g_logMutex);
        QTextStream out(&g_logFile);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        out.setEncoding(QStringConverter::Utf8);
#endif
        out << QDateTime::currentDateTime().toString(Qt::ISODateWithMs) << " ";
        switch (type) {
        case QtDebugMsg:    out << "[DEBUG] "; break;
        case QtInfoMsg:     out << "[INFO ] "; break;
        case QtWarningMsg:  out << "[WARN ] "; break;
        case QtCriticalMsg: out << "[ERROR] "; break;
        case QtFatalMsg:    out << "[FATAL] "; break;
        }
        out << msg << "\n";
        out.flush();
        g_logFile.flush();
    }

    if (g_prevHandler) {
        g_prevHandler(type, context, msg);
    } else {
        const QByteArray bytes = msg.toLocal8Bit();
        std::fwrite(bytes.constData(), 1, static_cast<size_t>(bytes.size()), stderr);
        std::fputc('\n', stderr);
        std::fflush(stderr);
    }
}

bool setFileLoggingEnabled(bool enabled)
{
    if (enabled) {
        if (!g_logFile.isOpen()) {
            g_logFile.setFileName(logFilePath());
            if (!g_logFile.open(QIODevice::Append | QIODevice::Text)) {
                return false;
            }
        }
        if (!g_prevHandler) {
            g_prevHandler = qInstallMessageHandler(fileLogHandler);
        } else {
            qInstallMessageHandler(fileLogHandler);
        }
        g_logEnabled = true;
        return true;
    }

    g_logEnabled = false;
    if (g_prevHandler) {
        qInstallMessageHandler(g_prevHandler);
    } else {
        qInstallMessageHandler(nullptr);
    }
    if (g_logFile.isOpen()) {
        g_logFile.flush();
    }
    return true;
}
// ================= 图片查看浮层（半透明背景 + 右上角关闭） =================


} // namespace

// 简单的“阅读设置”对话框：所有控件改动时实时发出 styleChanged 信号
class ReaderSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ReaderSettingsDialog(const ReaderStyle &initialStyle,
                                  int historyLimit,
                                  int recentLimit,
                                  QWidget *parent = nullptr)
        : QDialog(parent)
        , m_style(initialStyle)
        , m_historyLimit(historyLimit)
        , m_recentLimit(recentLimit)
    {
        setWindowTitle(QStringLiteral("阅读设置"));
        // 小工具窗 + 置顶，方便一边调一边看效果
        setWindowFlags(windowFlags()
                       | Qt::Tool
                       | Qt::WindowStaysOnTopHint);
        setModal(false);

        // ===== 字体大小 =====
        m_fontSizeSpin = new QSpinBox(this);
        m_fontSizeSpin->setRange(8, 40);
        m_fontSizeSpin->setValue(m_style.fontPointSize);
        connect(m_fontSizeSpin,
                QOverload<int>::of(&QSpinBox::valueChanged),
                this,
                [this](int v) {
                    m_style.fontPointSize = v;
                    emit styleChanged(m_style);   // 字号改动 -> 立刻通知外面
                });

        // ===== 背景透明度：0% ~ 100% =====
        m_opacitySlider = new QSlider(Qt::Horizontal, this);
        m_opacitySlider->setRange(0, 100);
        m_opacitySlider->setValue(
            static_cast<int>(m_style.backgroundOpacity * 100.0));

        auto *opacityLabel = new QLabel(this);
        opacityLabel->setMinimumWidth(40);

        auto updateOpacityLabel = [this, opacityLabel]() {
            const int v = m_opacitySlider->value();
            opacityLabel->setText(
                QString::number(v) + QStringLiteral("%"));
        };
        updateOpacityLabel();

        connect(m_opacitySlider, &QSlider::valueChanged,
                this, [this, opacityLabel](int v) {
                    if (v < 0) v = 0;
                    if (v > 100) v = 100;
                    m_opacitySlider->blockSignals(true);
                    m_opacitySlider->setValue(v);
                    m_opacitySlider->blockSignals(false);

                    m_style.backgroundOpacity = v / 100.0;
                    opacityLabel->setText(
                        QString::number(v) + QStringLiteral("%"));
                    emit styleChanged(m_style);   // 透明度实时生效
                });

        // ===== 字体透明度：20% ~ 100% =====
        m_fontOpacitySlider = new QSlider(Qt::Horizontal, this);
        m_fontOpacitySlider->setRange(20, 100);
        m_fontOpacitySlider->setValue(
            static_cast<int>(m_style.fontOpacity * 100.0));

        auto *fontOpacityLabel = new QLabel(this);
        fontOpacityLabel->setMinimumWidth(40);

        auto updateFontOpacityLabel = [this, fontOpacityLabel]() {
            const int v = m_fontOpacitySlider->value();
            fontOpacityLabel->setText(
                QString::number(v) + QStringLiteral("%"));
        };
        updateFontOpacityLabel();

        connect(m_fontOpacitySlider, &QSlider::valueChanged,
                this, [this, fontOpacityLabel](int v) {
                    if (v < 20) v = 20;
                    if (v > 100) v = 100;
                    m_fontOpacitySlider->blockSignals(true);
                    m_fontOpacitySlider->setValue(v);
                    m_fontOpacitySlider->blockSignals(false);

                    m_style.fontOpacity = v / 100.0;
                    fontOpacityLabel->setText(
                        QString::number(v) + QStringLiteral("%"));
                    emit styleChanged(m_style);   // 字体透明度实时生效
                });


        // ===== 字体颜色按钮 =====
        m_fontColorButton = new QPushButton(this);
        updateColorButton(m_fontColorButton, m_style.fontColor);
        connect(m_fontColorButton, &QPushButton::clicked,
                this, [this]() {
                    // 记录原始颜色，取消时可以恢复
                    const QColor original = m_style.fontColor;

                    auto *dlg = new QColorDialog(m_style.fontColor, this);
                    dlg->setOption(QColorDialog::ShowAlphaChannel, false);
                    dlg->setAttribute(Qt::WA_DeleteOnClose);

                    // 拖动过程中：实时预览 + 通知外面应用
                    connect(dlg, &QColorDialog::currentColorChanged,
                            this, [this](const QColor &c) {
                                if (!c.isValid()) return;
                                m_style.fontColor = c;
                                updateColorButton(m_fontColorButton,
                                                  m_style.fontColor);
                                emit styleChanged(m_style);
                            });

                    // 点 Cancel：恢复原色并通知外面恢复
                    connect(dlg, &QColorDialog::rejected,
                            this, [this, original]() {
                                m_style.fontColor = original;
                                updateColorButton(m_fontColorButton,
                                                  m_style.fontColor);
                                emit styleChanged(m_style);
                            });

                    // 点 OK：保持当前颜色（currentColorChanged
                    // 已经发过最终一次 styleChanged 了）
                    connect(dlg, &QColorDialog::accepted,
                            this, [this, dlg]() {
                                const QColor c = dlg->selectedColor();
                                if (!c.isValid()) return;
                                m_style.fontColor = c;
                                updateColorButton(m_fontColorButton,
                                                  m_style.fontColor);
                                emit styleChanged(m_style);
                            });

                    dlg->open();
                });

        // ===== 背景颜色按钮 =====
        m_backgroundColorButton = new QPushButton(this);
        updateColorButton(m_backgroundColorButton, m_style.backgroundColor);
        connect(m_backgroundColorButton, &QPushButton::clicked,
                this, [this]() {
                    const QColor original = m_style.backgroundColor;

                    auto *dlg = new QColorDialog(m_style.backgroundColor, this);
                    dlg->setOption(QColorDialog::ShowAlphaChannel, false);
                    dlg->setAttribute(Qt::WA_DeleteOnClose);

                    connect(dlg, &QColorDialog::currentColorChanged,
                            this, [this](const QColor &c) {
                                if (!c.isValid()) return;
                                m_style.backgroundColor = c;
                                updateColorButton(m_backgroundColorButton,
                                                  m_style.backgroundColor);
                                emit styleChanged(m_style);
                            });

                    connect(dlg, &QColorDialog::rejected,
                            this, [this, original]() {
                                m_style.backgroundColor = original;
                                updateColorButton(m_backgroundColorButton,
                                                  m_style.backgroundColor);
                                emit styleChanged(m_style);
                            });

                    connect(dlg, &QColorDialog::accepted,
                            this, [this, dlg]() {
                                const QColor c = dlg->selectedColor();
                                if (!c.isValid()) return;
                                m_style.backgroundColor = c;
                                updateColorButton(m_backgroundColorButton,
                                                  m_style.backgroundColor);
                                emit styleChanged(m_style);
                            });

                    dlg->open();
                });

        // ===== 表单布局 =====
        auto *form = new QFormLayout;
        form->addRow(QStringLiteral("字体大小"), m_fontSizeSpin);

        // 字体透明度一行（滑条 + 百分比标签）
        auto *fontOpacityRow = new QHBoxLayout;
        fontOpacityRow->setContentsMargins(0, 0, 0, 0);
        fontOpacityRow->addWidget(m_fontOpacitySlider, 1);
        // 注意：这里用的是上面构造函数里创建的 fontOpacityLabel 变量
        fontOpacityRow->addWidget(fontOpacityLabel);
        form->addRow(QStringLiteral("字体透明度"), fontOpacityRow);

        form->addRow(QStringLiteral("字体颜色"), m_fontColorButton);
        form->addRow(QStringLiteral("背景颜色"), m_backgroundColorButton);

        auto *opacityRow = new QHBoxLayout;
        opacityRow->setContentsMargins(0, 0, 0, 0);
        opacityRow->addWidget(m_opacitySlider, 1);
        opacityRow->addWidget(opacityLabel);
        form->addRow(QStringLiteral("背景透明度"), opacityRow);

                // 是否显示右侧滚动条
        m_scrollbarCheck = new QCheckBox(QStringLiteral("显示右侧滚动条"), this);
        m_scrollbarCheck->setChecked(m_style.showScrollbar);
        connect(m_scrollbarCheck, &QCheckBox::toggled,
                this, [this](bool checked) {
                    m_style.showScrollbar = checked;
                    emit styleChanged(m_style);
                });
        form->addRow(QStringLiteral("滚动条"), m_scrollbarCheck);

        // 历史记录上限
        m_historyLimitSpin = new QSpinBox(this);
        m_historyLimitSpin->setRange(1, 200);
        m_historyLimitSpin->setValue(m_historyLimit);
        connect(m_historyLimitSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int v) {
                    m_historyLimit = v;
                    emit historyLimitChanged(v);
                });
        form->addRow(QStringLiteral("历史记录上限"), m_historyLimitSpin);

        // 最近打开上限
        m_recentLimitSpin = new QSpinBox(this);
        m_recentLimitSpin->setRange(1, 200);
        m_recentLimitSpin->setValue(m_recentLimit);
        connect(m_recentLimitSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int v) {
                    m_recentLimit = v;
                    emit recentLimitChanged(v);
                });
        form->addRow(QStringLiteral("最近打开上限"), m_recentLimitSpin);


        // 底部一个“关闭”按钮
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
        connect(buttons, &QDialogButtonBox::rejected,
                this, &QDialog::reject);
        connect(buttons, &QDialogButtonBox::accepted,
                this, &QDialog::accept);

        auto *mainLayout = new QVBoxLayout(this);
        mainLayout->addLayout(form);
        mainLayout->addWidget(buttons);
        setLayout(mainLayout);

        resize(380, sizeHint().height());
    }

signals:
    // 每次用户改动任一控件都会发出一次
    void styleChanged(const ReaderStyle &style);
    void historyLimitChanged(int limit);
    void recentLimitChanged(int limit);

private:
    static void updateColorButton(QPushButton *btn, const QColor &color)
    {
        if (!btn) return;
        const QString style = QStringLiteral(
            "QPushButton { background-color: %1; border: 1px solid #444; }")
            .arg(color.name(QColor::HexRgb));
        btn->setText(QString());
        btn->setStyleSheet(style);
        btn->setFixedWidth(60);
    }

    ReaderStyle  m_style;
    QSpinBox    *m_fontSizeSpin          = nullptr;
    QSlider     *m_fontOpacitySlider     = nullptr;   // 字体透明度
    QSlider     *m_opacitySlider         = nullptr;   // 背景透明度
    QPushButton *m_fontColorButton       = nullptr;
    QPushButton *m_backgroundColorButton = nullptr;
    QCheckBox   *m_scrollbarCheck        = nullptr;
    QSpinBox    *m_historyLimitSpin      = nullptr;
    QSpinBox    *m_recentLimitSpin       = nullptr;
    int          m_historyLimit          = 20;
    int          m_recentLimit           = 20;
};

class MainWindow;   // 前向声明

// ================= 浮动按钮条：Settings + X =================
class TitleBar : public QWidget
{
public:
    explicit TitleBar(MainWindow *owner)
        : QWidget(nullptr)
        , m_mainWindow(owner)
    {
        // 顶层小工具窗，始终在最前，不占任务栏
        setWindowFlags(Qt::FramelessWindowHint
                       | Qt::Tool
                       | Qt::WindowStaysOnTopHint);
        setAttribute(Qt::WA_TranslucentBackground);
        setFixedHeight(40);

        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(8, 0, 8, 0);
        layout->setSpacing(6);

        // 左下角：拖动按钮（按住左键可拖动主窗口）
        m_dragButton = new QToolButton(this);
        m_dragButton->setText(QStringLiteral("📄 TransparentMdReader"));
        m_dragButton->setToolTip(QStringLiteral("按住左键拖动窗口位置"));
        m_dragButton->setAutoRaise(true);
        m_dragButton->setCursor(Qt::SizeAllCursor);
        layout->addWidget(m_dragButton, 1);


        // 历史记录：上一篇 / 下一篇
        m_prevDocButton = new QToolButton(this);
        m_prevDocButton->setText(QStringLiteral("上一篇"));
        m_prevDocButton->setToolTip(QStringLiteral("历史记录后退到上一文件"));
        layout->addWidget(m_prevDocButton);

        m_nextDocButton = new QToolButton(this);
        m_nextDocButton->setText(QStringLiteral("下一篇"));
        m_nextDocButton->setToolTip(QStringLiteral("历史记录前进到下一文件"));
        layout->addWidget(m_nextDocButton);

        // 锁定状态按钮：🔒 / 🔓
        m_lockButton = new QToolButton(this);
        m_lockButton->setText(QStringLiteral("🔒"));
        m_lockButton->setToolTip(
            QStringLiteral("当前已锁定（鼠标穿透）。按住 Ctrl 可临时解锁，或点击此按钮解除锁定。"));
        layout->addWidget(m_lockButton);

        // 一屏翻页按钮（上一屏 / 下一屏）
        m_prevPageButton = new QToolButton(this);
        m_prevPageButton->setText(QStringLiteral("▲"));
        m_prevPageButton->setToolTip(QStringLiteral("上一屏（向上翻页）"));
        layout->addWidget(m_prevPageButton);

        m_nextPageButton = new QToolButton(this);
        m_nextPageButton->setText(QStringLiteral("▼"));
        m_nextPageButton->setToolTip(QStringLiteral("下一屏（向下翻页）"));
        layout->addWidget(m_nextPageButton);

        // Settings 按钮
        m_settingsButton = new QToolButton(this);
        m_settingsButton->setText(QStringLiteral("Settings"));
        layout->addWidget(m_settingsButton);

        // 关闭按钮
        m_closeButton = new QToolButton(this);
        m_closeButton->setText(QStringLiteral("✕"));
        m_closeButton->setToolTip(QStringLiteral("关闭阅读器"));
        layout->addWidget(m_closeButton);

            // 统一放大几个按钮
        auto enlargeButton = [](QToolButton *btn) {
            if (!btn) return;
            // 最小宽高稍微大一点
            btn->setMinimumSize(36, 28);
            // 字体放大一点
            QFont f = btn->font();
            f.setPointSize(f.pointSize() + 2);
            btn->setFont(f);
        };

        enlargeButton(m_dragButton);
        enlargeButton(m_prevDocButton);
        enlargeButton(m_nextDocButton);
        enlargeButton(m_prevPageButton);
        enlargeButton(m_nextPageButton);
        enlargeButton(m_lockButton);
        enlargeButton(m_settingsButton);
        enlargeButton(m_closeButton);


        // 关闭阅读器
        connect(m_closeButton, &QToolButton::clicked, this, [this]() {
            if (m_mainWindow) {
                m_mainWindow->close();
            }
            close();
        });

        // Settings：打开阅读设置对话框（实时预览）
        connect(m_settingsButton, &QToolButton::clicked, this, [this]() {
            if (m_mainWindow) {
                m_mainWindow->openSettingsDialog();
            }
        });

        // 左下角拖动按钮：按住左键可以拖动窗口
        connect(m_dragButton, &QToolButton::pressed, this, [this]() {
            if (!m_mainWindow) return;
#ifdef Q_OS_WIN
            HWND hwnd = reinterpret_cast<HWND>(m_mainWindow->winId());
            if (hwnd) {
                ReleaseCapture();
                SendMessageW(hwnd,
                             WM_SYSCOMMAND,
                             SC_MOVE | HTCAPTION,
                             0);
            }
#else
            // 非 Windows 平台：这里暂时不做特殊处理
#endif
        });


        // 翻页按钮：始终可用（不受锁定影响）
        connect(m_prevPageButton, &QToolButton::clicked, this, [this]() {
            if (m_mainWindow) {
                m_mainWindow->scrollPageUp();
            }
        });
        connect(m_nextPageButton, &QToolButton::clicked, this, [this]() {
            if (m_mainWindow) {
                m_mainWindow->scrollPageDown();
            }
        });

        // 历史记录：上一篇 / 下一篇（挂在已有的 goBack/goForward 上）
        connect(m_prevDocButton, &QToolButton::clicked, this, [this]() {
            if (m_mainWindow) {
                m_mainWindow->goBack();
            }
        });
        connect(m_nextDocButton, &QToolButton::clicked, this, [this]() {
            if (m_mainWindow) {
                m_mainWindow->goForward();
            }
        });

        // 🔒 按钮：切换用户锁定偏好
        connect(m_lockButton, &QToolButton::clicked, this, [this]() {
            if (m_mainWindow) {
                m_mainWindow->toggleLockByUser();
            }
        });
    }

    // 根据当前锁定状态更新按钮外观与提示
    void syncFromWindowLockState(bool locked)
    {
        if (m_lockButton) {
            m_lockButton->setText(locked ? QStringLiteral("🔒")
                                         : QStringLiteral("🔓"));
            if (locked) {
                m_lockButton->setToolTip(
                    QStringLiteral("当前已锁定（鼠标穿透）。按住 Ctrl 可临时解锁，或点击此按钮解除锁定。"));
            } else {
                m_lockButton->setToolTip(
                    QStringLiteral("当前已解锁。松开 Ctrl 或再次点击此按钮可恢复锁定（鼠标穿透）。"));
            }
        }

        if (m_settingsButton) {
            if (locked) {
                m_settingsButton->setToolTip(
                    QStringLiteral("当前已锁定（鼠标穿透）。按住 Ctrl 可临时解锁。"));
            } else {
                m_settingsButton->setToolTip(
                    QStringLiteral("当前已解锁，内容可交互。"));
            }
        }
    }

    // 把标题栏贴到主窗口底部
    void syncWithMainWindow()
    {
        if (!m_mainWindow) return;
        const QRect frame = m_mainWindow->frameGeometry();
        setFixedWidth(frame.width());
        // 紧贴主窗口下边缘（在阅读窗口下面一条）
        move(frame.left(), frame.bottom());
    }

    // 根据阅读样式同步按钮的颜色 / 透明度
    void applyReaderUiStyle(const ReaderStyle &style)
{
        // 前景色：跟随字体颜色；透明度跟随“字体透明度”
        QColor fg = style.fontColor;
        qreal alpha = style.fontOpacity;
        if (alpha < 0.0) alpha = 0.0;
        if (alpha > 1.0) alpha = 1.0;
        fg.setAlphaF(alpha);

        const QString cssColor = QStringLiteral("rgba(%1,%2,%3,%4)")
                                    .arg(fg.red())
                                    .arg(fg.green())
                                    .arg(fg.blue())
                                    .arg(fg.alphaF());

        const QString btnStyle = QStringLiteral(
            "QToolButton {"
            "  background-color: transparent;"
            "  border: none;"
            "  color: %1;"
            "}"
            "QToolButton:hover {"
            "  background-color: rgba(255, 255, 255, 0.08);"
            "}"
            "QToolButton:pressed {"
            "  background-color: rgba(255, 255, 255, 0.16);"
            "}"
        ).arg(cssColor);

        // 应用到整条 TitleBar 上，所有 QToolButton 子控件都会继承
        setStyleSheet(btnStyle);
    }


private:
    MainWindow  *m_mainWindow      = nullptr;
    QToolButton *m_dragButton      = nullptr;
    QToolButton *m_prevPageButton  = nullptr;
    QToolButton *m_nextPageButton  = nullptr;
    QToolButton *m_prevDocButton   = nullptr;
    QToolButton *m_nextDocButton   = nullptr;
    QToolButton *m_lockButton      = nullptr;
    QToolButton *m_settingsButton  = nullptr;
    QToolButton *m_closeButton     = nullptr;
};



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

// 文件：src/app/MainWindow.cpp（构造函数）
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // 无边框 + 置顶 + 透明背景
    // 只保留我们需要的 flags：一个顶层透明窗，不要系统标题栏
    setWindowFlags(Qt::FramelessWindowHint
                   | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    // 保持窗口自身不透明（1.0），背景透明度交给前端样式控制
    setWindowOpacity(1.0);


    resize(720, 900);
    setMinimumSize(480, 600);

    // 中央容器：只放 WebEngine 区域
    auto *central = new QWidget(this);
    central->setAttribute(Qt::WA_TranslucentBackground);

    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(0);

    m_view = new QWebEngineView(central);
    // 让 Web 内容本身背景透明，避免露出默认的白底
    m_view->setAttribute(Qt::WA_TranslucentBackground);
    m_view->setStyleSheet(QStringLiteral("background: transparent;"));

    m_view->setContextMenuPolicy(Qt::NoContextMenu);
    m_view->installEventFilter(this);
    layout->addWidget(m_view, 1);

    setCentralWidget(central);

    // 定时读取页面滚动比例并写入状态库（节流）
    m_scrollTimer = new QTimer(this);
    m_scrollTimer->setInterval(500);
    connect(m_scrollTimer, &QTimer::timeout, this, [this]() {
        if (!m_view || !m_view->page() || m_currentFilePath.isEmpty()) {
            return;
        }
        if (!m_pageLoaded) {
            return;
        }
        if (m_restoringScroll) {
            return;
        }
        if (m_openingFile) {
            return;
        }
        const QString js = QStringLiteral(
            R"JS(
(() => {
  const candidates = [
    document.scrollingElement,
    document.documentElement,
    document.body,
    document.getElementById('md-root'),
    ...document.querySelectorAll('.md-root, .markdown-body')
  ];
  for (const el of candidates) {
    if (!el) continue;
    const max = el.scrollHeight - el.clientHeight;
    if (max > 1) {
      return Math.max(0, Math.min(1, el.scrollTop / max));
    }
  }
  const doc = document.documentElement;
  const max = Math.max(1, doc.scrollHeight - window.innerHeight);
  return Math.max(0, Math.min(1, window.scrollY / max));
})();
)JS");
        m_view->page()->runJavaScript(js, [this](const QVariant &v) {
            const double ratio = v.toDouble();
            if (ratio < 0.0 || ratio > 1.0) {
                return;
            }
            if (qAbs(ratio - m_lastScrollRatio) < 0.001) {
                return;
            }
            m_lastScrollRatio = ratio;
            StateDbManager::instance().updateScroll(m_currentFilePath, ratio);
        });
    });
    m_scrollTimer->start();

    // 初始化状态数据库（使用默认路径）
    StateDbManager::instance().open();

    // 创建浮动按钮条（上一篇/下一篇/翻页/锁定/设置/关闭）
    m_titleBar = new TitleBar(this);
    m_titleBar->syncFromWindowLockState(m_locked);
    m_titleBar->syncWithMainWindow();
    m_titleBar->applyReaderUiStyle(g_readerStyle);  // 初始同步按钮的颜色 / 透明度
    m_titleBar->show();




    // 使用自定义 QWebEnginePage（MarkdownPage）拦截链接点击
    auto *page = new MarkdownPage(m_view);
    page->setBackgroundColor(Qt::transparent);  // 底色改成透明
    m_view->setPage(page);
    connect(page, &MarkdownPage::openMarkdown,
            this, &MainWindow::handleOpenMarkdownUrl);

    connect(page, &MarkdownPage::openImage,
            this, &MainWindow::handleOpenImageUrl);

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

    // 加载历史记录（仅调用一次）
    loadHistoryFromSettings();

    m_recentLimit = settings.value("recent/limit", 20).toInt();
    if (m_recentLimit < 1)  m_recentLimit = 20;
    if (m_recentLimit > 200) m_recentLimit = 200;

g_readerStyle.fontPointSize =
        settings.value("reader/fontPointSize", g_readerStyle.fontPointSize).toInt();

    const QString fontColorStr = settings.value("reader/fontColor").toString();
    if (!fontColorStr.isEmpty()) {
        const QColor c(fontColorStr);
        if (c.isValid()) {
            g_readerStyle.fontColor = c;
        }
    }

        // 新增：字体透明度
    g_readerStyle.fontOpacity =
            settings.value("reader/fontOpacity", 1.0).toDouble();
    if (g_readerStyle.fontOpacity < 0.2) g_readerStyle.fontOpacity = 0.2;   // 不允许完全看不见
    if (g_readerStyle.fontOpacity > 1.0) g_readerStyle.fontOpacity = 1.0;

    const QString bgColorStr = settings.value("reader/backgroundColor").toString();
    if (!bgColorStr.isEmpty()) {
        const QColor c(bgColorStr);
        if (c.isValid()) {
            g_readerStyle.backgroundColor = c;
        }
    }

    g_readerStyle.backgroundOpacity =
        settings.value("reader/backgroundOpacity", g_readerStyle.backgroundOpacity).toDouble();
    if (g_readerStyle.backgroundOpacity < 0.0) g_readerStyle.backgroundOpacity = 0.0;
    if (g_readerStyle.backgroundOpacity > 1.0) g_readerStyle.backgroundOpacity = 1.0;

    g_readerStyle.showScrollbar =
        settings.value("reader/showScrollbar", g_readerStyle.showScrollbar).toBool();

    m_manualLocked = settings.value("reader/manualLocked", true).toBool();

    m_autoStartEnabled = queryAutoStartEnabled();
    m_loggingEnabled = settings.value("logging/enabled", false).toBool();
    if (m_loggingEnabled && !setFileLoggingEnabled(true)) {
        m_loggingEnabled = false;
    }


    const QUrl pageUrl = locateIndexPage();
    if (pageUrl.isValid()) {
        m_useEmbeddedViewer = true;
        m_view->load(pageUrl);
        connect(m_view, &QWebEngineView::loadFinished,
                this, [this](bool ok) {
                    m_pageLoaded = ok;
                    if (!ok) {
                        // 本次加载失败，结束“正在打开文件”状态
                        m_openingFile = false;
                        return;
                    }

                    // 重新把阅读样式同步给前端
                    applyReaderStyle();

                    // ??? pending ????????
                    if (!m_pendingMarkdown.isEmpty()) {
                        const QUrl baseUrl(m_pendingBaseUrl);
                        renderMarkdownInPage(m_pendingMarkdown,
                                             m_pendingTitle,
                                             baseUrl);
                        m_pendingMarkdown.clear();
                        m_pendingTitle.clear();
                        m_pendingBaseUrl.clear();
                    }

                    if (!m_currentFilePath.isEmpty()) {
                        double ratio = 0.0;
                        if (m_pendingScrollRatio > 0.001) {
                            ratio = m_pendingScrollRatio;
                        } else {
                            ratio = StateDbManager::instance().loadScroll(m_currentFilePath);
                        }

                        m_pendingScrollRatio = 0.0;
                        m_lastScrollRatio    = ratio;
                        if (m_lastScrollRatio > 0.001) {
                            applyScrollRatio(m_lastScrollRatio);
                        }
                    }

                    // ????????????????
                    m_openingFile = false;
                });
    } else {
        m_useEmbeddedViewer = false;
        m_pageLoaded        = true;

        // 使用占位 HTML 渲染
        const QString html = placeholderHtml();
        m_view->setHtml(html);
        applyReaderStyle();
    }
    createSystemTray();
    autoOpenLastFileIfNeeded();

// NEW: Windows 下注册全局热键 Ctrl+Alt+L，用来锁定/解锁
// #ifdef Q_OS_WIN
//     {
//         HWND hwnd = reinterpret_cast<HWND>(winId());  // 确保创建 HWND
//         if (hwnd) {
//             // 热键 ID = 1，对应 nativeEvent 里 WM_HOTKEY 分支
//             // MOD_CONTROL | MOD_ALT + 'L'
//             RegisterHotKey(hwnd, 1, MOD_CONTROL | MOD_ALT, 'L');
//         }
//     }
// #endif

        // NEW: 启动时按照用户偏好锁定 / 内容穿透
    setLocked(m_manualLocked);

#ifdef Q_OS_WIN
    // 每 30ms 轮询一次 Ctrl 键状态：
    //  - Ctrl 未按下：跟随用户的锁定偏好 m_manualLocked
    //  - Ctrl 按下：一律临时解锁（可交互）
    auto *ctrlTimer = new QTimer(this);
    ctrlTimer->setInterval(30);
    connect(ctrlTimer, &QTimer::timeout, this, [this]() {
        SHORT state = GetAsyncKeyState(VK_CONTROL);
        bool ctrlDown = (state & 0x8000) != 0;

        // 默认使用用户的锁定偏好
        bool effectiveLocked = m_manualLocked;

        // 按住 Ctrl 时临时解锁
        if (ctrlDown) {
            effectiveLocked = false;
        }

        if (effectiveLocked != m_locked) {
            setLocked(effectiveLocked);
        }
    });
    ctrlTimer->start();
#endif


    // 这里原来如果有 Ctrl+O 快捷键等，保持不动
    auto *openShortcut = new QShortcut(QKeySequence::Open, this);
    connect(openShortcut, &QShortcut::activated,
            this, &MainWindow::openMarkdownFileFromDialog);
}


MainWindow::~MainWindow()
{
    if (m_loggingEnabled) {
        setFileLoggingEnabled(false);
    }
#ifdef Q_OS_WIN
    // 释放 Ctrl+Alt+L 这个热键（ID = 1）
    HWND hwnd = reinterpret_cast<HWND>(winId());
    if (hwnd) {
        UnregisterHotKey(hwnd, 1);
    }
#endif
}

void MainWindow::openSettingsDialog()
{
    // 每次点击 Settings 新建一个对话框，关闭后自动 delete
    auto *dlg = new ReaderSettingsDialog(g_readerStyle, m_historyLimit, m_recentLimit, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    // 实时更新：任何控件变化都会发出 styleChanged
    connect(dlg, &ReaderSettingsDialog::styleChanged,
            this, [this](const ReaderStyle &style) {
                g_readerStyle = style;
                applyReaderStyle();

                if (m_titleBar) {
                m_titleBar->applyReaderUiStyle(g_readerStyle);   // 第 3 点会用到，放在一起
            }

                // 同步保存到 QSettings，重启后仍然生效
                QSettings settings("zhiz", "TransparentMdReader");
                settings.setValue("reader/fontPointSize", g_readerStyle.fontPointSize);
                settings.setValue("reader/fontColor", g_readerStyle.fontColor.name(QColor::HexRgb));
                settings.setValue("reader/fontOpacity", g_readerStyle.fontOpacity);
                settings.setValue("reader/backgroundColor", g_readerStyle.backgroundColor.name(QColor::HexRgb));
                settings.setValue("reader/backgroundOpacity", g_readerStyle.backgroundOpacity);
                settings.setValue("reader/showScrollbar", g_readerStyle.showScrollbar);
            });
    connect(dlg, &ReaderSettingsDialog::historyLimitChanged,
            this, [this](int limit) {
                m_historyLimit = qBound(1, limit, 200);
                trimHistory();
                persistHistory();
            });
    connect(dlg, &ReaderSettingsDialog::recentLimitChanged,
            this, [this](int limit) {
                m_recentLimit = qBound(1, limit, 200);
                QSettings settings("zhiz", "TransparentMdReader");
                settings.setValue("recent/limit", m_recentLimit);
                rebuildRecentMenu();
            });

    dlg->show();
    dlg->raise();
    dlg->activateWindow();
}

void MainWindow::applyReaderStyle()
{
    if (!m_view || !m_view->page()) return;

    const QString fontSizeCss =
        QString::number(g_readerStyle.fontPointSize) + QStringLiteral("px");
    const QString fontColorCss =
        colorToCssRgba(g_readerStyle.fontColor, g_readerStyle.fontOpacity);
    // 当背景透明度为 0 时，强制使用完全透明色，避免底层残留颜色
    QString bgCss;
    if (g_readerStyle.backgroundOpacity <= 0.0001) {
        bgCss = QStringLiteral("rgba(0,0,0,0)");
    } else {
        bgCss = colorToCssRgba(g_readerStyle.backgroundColor, g_readerStyle.backgroundOpacity);
    }
    // 边框与阴影：随背景透明度线性衰减，避免低透明时灰色残留
    QString borderCss;
    QString shadowCss;
    if (g_readerStyle.backgroundOpacity <= 0.0001) {
        borderCss = QStringLiteral("1px solid rgba(0,0,0,0)");
        shadowCss = QStringLiteral("0 0 0 rgba(0,0,0,0)");
    } else {
        const qreal borderAlpha = qBound<qreal>(0.0, 0.18 * g_readerStyle.backgroundOpacity, 1.0);
        const qreal shadowAlpha = qBound<qreal>(0.0, 0.45 * g_readerStyle.backgroundOpacity, 1.0);
        borderCss = QStringLiteral("1px solid %1").arg(colorToCssRgba(g_readerStyle.backgroundColor, borderAlpha));
        shadowCss = QStringLiteral("0 18px 40px rgba(0,0,0,%1)").arg(shadowAlpha, 0, 'f', 3);
    }
    const QString scrollbarWidthCss =
        g_readerStyle.showScrollbar ? QStringLiteral("8px") : QStringLiteral("0px");
    // 隐藏滚动条但仍允许滚动，使用 auto+宽度0 兼顾滚轮/翻页按钮
    const QString overflowCss =
        g_readerStyle.showScrollbar ? QStringLiteral("overlay") : QStringLiteral("auto");

    const QString js = QStringLiteral(
        "(function(){"
        "  var fontSize = '%1';"
        "  var fontColor = '%2';"
        "  var bg = '%3';"
        "  var border = '%4';"
        "  var shadow = '%5';"
        "  var scrollbarWidth = '%6';"
        "  var overflowY = '%7';"
        "  var styleId = 'tmr-reader-style';"
        "  var doc = document;"
        "  var head = doc.head || doc.getElementsByTagName('head')[0];"
        "  if (!head) return;"
        "  var style = doc.getElementById(styleId);"
        "  if (!style) { style = doc.createElement('style'); style.id = styleId; head.appendChild(style); }"
        "  var css = '';"

        // 1) 全局变量
        "  css += ':root{--md-font-size:' + fontSize + ';"
        "                 --md-bg:' + bg + ';"
        "                 --md-border:' + border + ';"
        "                 --md-shadow:' + shadow + ';"
        "                 --md-fg:' + fontColor + ';}';"

        // 2) 页面根与常见容器：颜色/字号/背景同时兜底
        "  css += 'html, body, #md-root, .md-root, #root, #app, .markdown-body, .md-content{"
        "           font-size:var(--md-font-size);"
        "           color:var(--md-fg);"
        "         }';"
        "  css += 'body, #md-root, .md-root{"
        "           background: var(--md-bg) !important;"
        "         }';"
        "  css += '#md-root, .md-root{"
        "           border: var(--md-border) !important;"
        "           box-shadow: var(--md-shadow) !important;"
        "         }';"

                // 3) 显式控制滚动条（命中 html / body / .md-root 三种容器）
        "  css += 'html::-webkit-scrollbar, body::-webkit-scrollbar, .md-root::-webkit-scrollbar{"
        "           width:' + scrollbarWidth + ';"
        "         }';"

        // 滚动条轨道：透明，不抢背景
        "  css += 'html::-webkit-scrollbar-track, body::-webkit-scrollbar-track, .md-root::-webkit-scrollbar-track{"
        "           background-color: rgba(0,0,0,0);"
        "         }';"

        // 滚动条 thumb：用一个固定的浅色，始终可见
        "  css += 'html::-webkit-scrollbar-thumb, body::-webkit-scrollbar-thumb, .md-root::-webkit-scrollbar-thumb{"
        "           background-color: rgba(255, 255, 255, 0.35);"
        "           border-radius: 6px;"
        "         }';"

        // 控制是否出现滚动条（有些平台 overlay 会保留滚轮）
        "  css += 'html, body{overflow-y:' + overflowY + ';}';"

        "  style.textContent = css;"
        "})();"
    ).arg(fontSizeCss, fontColorCss, bgCss, borderCss, shadowCss, scrollbarWidthCss, overflowCss);

    m_view->page()->runJavaScript(js);
}


void MainWindow::toggleLockByUser()
{
    // 用户点击标题栏上的 🔒 按钮时调用：
    // 切换“基础锁定偏好”，Ctrl 仍然可以临时解锁
    m_manualLocked = !m_manualLocked;
    QSettings settings("zhiz", "TransparentMdReader");
    settings.setValue("reader/manualLocked", m_manualLocked);

#ifdef Q_OS_WIN
    // 立即按当前 Ctrl 状态 + 用户偏好应用一次，避免感觉迟钝
    SHORT state = GetAsyncKeyState(VK_CONTROL);
    bool ctrlDown = (state & 0x8000) != 0;

    bool effectiveLocked = m_manualLocked;
    if (ctrlDown) {
        // 按住 Ctrl 时一律视为解锁
        effectiveLocked = false;
    }
    setLocked(effectiveLocked);
#else
    setLocked(m_manualLocked);
#endif
}


// NEW: 统一处理锁定状态（同步 TitleBar 外观）
void MainWindow::setLocked(bool locked)
{
    if (m_locked == locked) {
        return;
    }

    m_locked = locked;
    updateClickThroughState();

    if (m_titleBar) {
        m_titleBar->syncFromWindowLockState(m_locked);
    }
}

void MainWindow::updateClickThroughState()
{
#ifdef Q_OS_WIN
    // 锁定 = 整窗鼠标穿透；解锁 = 正常可交互
    setWindowClickThrough(this, m_locked);
#else
    // 其他平台暂时不做特殊处理
#endif
}





// NEW: Windows 下通过 WM_NCHITTEST 实现“内容区域点击穿透”
bool MainWindow::nativeEvent(const QByteArray &eventType,
                             void *message,
                             qintptr *result)
{
#ifdef Q_OS_WIN
    Q_UNUSED(eventType)
    Q_UNUSED(message)
    Q_UNUSED(result)
#endif
    return QMainWindow::nativeEvent(eventType, message, result);
}




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

void MainWindow::closeEvent(QCloseEvent *event)
{
    // 当系统托盘可用时，默认“关闭”仅隐藏窗口，保持托盘常驻
    if (!m_exiting && QSystemTrayIcon::isSystemTrayAvailable()) {
        event->ignore();
        hide();
        return;
    }

    QMainWindow::closeEvent(event);
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    if (m_titleBar) {
        m_titleBar->syncWithMainWindow();
        m_titleBar->syncFromWindowLockState(m_locked);
        m_titleBar->applyReaderUiStyle(g_readerStyle);
        m_titleBar->show();
        m_titleBar->raise();
    }
}

void MainWindow::hideEvent(QHideEvent *event)
{
    QMainWindow::hideEvent(event);
    if (m_titleBar) {
        m_titleBar->hide();
    }
}

void MainWindow::createSystemTray()
{
    if (m_trayIcon) {
        return;
    }

    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        return;
    }

    m_trayIcon = new QSystemTrayIcon(this);
    QIcon trayIcon = windowIcon();
    if (trayIcon.isNull()) {
        trayIcon = style()->standardIcon(QStyle::SP_FileIcon);
    }
    m_trayIcon->setIcon(trayIcon);
    m_trayIcon->setToolTip(QStringLiteral("TransparentMdReader"));

    m_trayMenu = new QMenu(this);
    m_trayOpenAction = m_trayMenu->addAction(QStringLiteral("打开 Markdown 文件..."));
    connect(m_trayOpenAction, &QAction::triggered,
            this, &MainWindow::openMarkdownFileFromDialog);

    m_recentMenu = m_trayMenu->addMenu(QStringLiteral("最近打开"));
    rebuildRecentMenu();

    m_trayAutoStartAction = m_trayMenu->addAction(QStringLiteral("开机自启"));
    m_trayAutoStartAction->setCheckable(true);
    connect(m_trayAutoStartAction, &QAction::toggled,
            this, &MainWindow::toggleAutoStart);

    m_trayLoggingAction = m_trayMenu->addAction(QStringLiteral("日志记录"));
    m_trayLoggingAction->setCheckable(true);
    connect(m_trayLoggingAction, &QAction::toggled,
            this, &MainWindow::toggleLogging);

    m_trayMenu->addSeparator();
    m_trayQuitAction = m_trayMenu->addAction(QStringLiteral("退出"));
    connect(m_trayQuitAction, &QAction::triggered,
            this, &MainWindow::quitFromTray);

    m_trayIcon->setContextMenu(m_trayMenu);
    connect(m_trayIcon, &QSystemTrayIcon::activated,
            this, &MainWindow::handleTrayActivated);

    updateTrayChecks();
    m_trayIcon->show();
}

void MainWindow::rebuildRecentMenu()
{
    if (!m_recentMenu) {
        return;
    }

    m_recentMenu->clear();

    const auto recents = StateDbManager::instance().listRecent(m_recentLimit);
    int added = 0;
    for (const auto &entry : recents) {
        QFileInfo fi(entry.path);
        if (!fi.exists() || !fi.isFile()) {
            StateDbManager::instance().markMissing(entry.path);
            continue;
        }
        const QString finalPath = fi.canonicalFilePath().isEmpty()
                                      ? fi.absoluteFilePath()
                                      : fi.canonicalFilePath();
        QString text = fi.fileName();
        if (text.isEmpty()) {
            text = finalPath;
        }

        QAction *act = m_recentMenu->addAction(text);
        act->setToolTip(finalPath);
        connect(act, &QAction::triggered, this, [this, finalPath]() {
            openMarkdownFile(finalPath);
        });
        ++added;
    }

    if (added == 0) {
        QAction *placeholder = m_recentMenu->addAction(QStringLiteral("(无最近文件)"));
        placeholder->setEnabled(false);
    }

    if (!m_trayClearRecentAction) {
        m_trayClearRecentAction = new QAction(QStringLiteral("清空最近列表"), this);
        connect(m_trayClearRecentAction, &QAction::triggered, this, [this]() {
            // 仅清除 last_open_time，不触碰滚动等其他字段
            StateDbManager::instance().clearRecent();
            rebuildRecentMenu();
        });
    }

    m_recentMenu->addSeparator();
    m_recentMenu->addAction(m_trayClearRecentAction);
}

void MainWindow::updateTrayChecks()
{
    if (m_trayAutoStartAction) {
        const QSignalBlocker blocker(m_trayAutoStartAction);
        m_trayAutoStartAction->setChecked(m_autoStartEnabled);
    }
    if (m_trayLoggingAction) {
        const QSignalBlocker blocker(m_trayLoggingAction);
        m_trayLoggingAction->setChecked(m_loggingEnabled);
    }
}

void MainWindow::handleTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (!m_trayIcon) {
        return;
    }

    if (reason == QSystemTrayIcon::Trigger
        || reason == QSystemTrayIcon::DoubleClick) {
        if (isHidden() || isMinimized()) {
            show();
            showNormal();
            raise();
            activateWindow();
        } else {
            hide();
        }
    }
}

void MainWindow::toggleAutoStart(bool enabled)
{
    QString error;
    if (!applyAutoStartEnabled(enabled, error)) {
        QMessageBox::warning(this,
                             QStringLiteral("开机自启"),
                             error);
        m_autoStartEnabled = queryAutoStartEnabled();
    } else {
        m_autoStartEnabled = enabled;
        QSettings settings("zhiz", "TransparentMdReader");
        settings.setValue("app/autoStart", m_autoStartEnabled);
    }
    updateTrayChecks();
}

void MainWindow::toggleLogging(bool enabled)
{
    if (!setFileLoggingEnabled(enabled)) {
        QMessageBox::warning(this,
                             QStringLiteral("日志记录"),
                             QStringLiteral("无法写入日志文件，请检查路径权限。"));
        m_loggingEnabled = false;
    } else {
        m_loggingEnabled = enabled;
        QSettings settings("zhiz", "TransparentMdReader");
        settings.setValue("logging/enabled", m_loggingEnabled);
    }
    updateTrayChecks();
}

void MainWindow::quitFromTray()
{
    m_exiting = true;
    QApplication::quit();
}

bool MainWindow::canGoBack() const
{
    return m_historyIndex > 0
        && m_historyIndex < m_history.size();
}

bool MainWindow::canGoForward() const
{
    return m_historyIndex >= 0
        && m_historyIndex < m_history.size() - 1;
}

void MainWindow::updateNavigationActions()
{
    if (m_backAction) {
        m_backAction->setEnabled(canGoBack());
    }
    if (m_forwardAction) {
        m_forwardAction->setEnabled(canGoForward());
    }
}

void MainWindow::goBack()
{
    if (!canGoBack()) {
        updateNavigationActions();
        return;
    }

    const int targetIndex = m_historyIndex - 1;
    const QString targetPath = m_history.at(targetIndex);
    if (openMarkdownFile(targetPath, false)) {
        m_historyIndex = targetIndex;
    }
    updateNavigationActions();
    persistHistory();
}

void MainWindow::goForward()
{
    if (!canGoForward()) {
        updateNavigationActions();
        return;
    }

    const int targetIndex = m_historyIndex + 1;
    const QString targetPath = m_history.at(targetIndex);
    if (openMarkdownFile(targetPath, false)) {
        m_historyIndex = targetIndex;
    }
    updateNavigationActions();
    persistHistory();
}

void MainWindow::showContextMenu(const QPoint &pos)
{
    if (!m_view) {
        return;
    }

    QMenu menu(this);
    if (m_backAction) {
        menu.addAction(m_backAction);
    }
    if (m_forwardAction) {
        menu.addAction(m_forwardAction);
    }

    menu.addSeparator();

    static const QWebEnginePage::WebAction kDefaultActions[] = {
        QWebEnginePage::Copy,
        QWebEnginePage::Paste,
        QWebEnginePage::SelectAll
    };

    for (QWebEnginePage::WebAction actionId : kDefaultActions) {
        QAction *action = m_view->pageAction(actionId);
        if (action) {
            menu.addAction(action);
        }
    }

    menu.exec(m_view->mapToGlobal(pos));
}

// 文件：src/app/MainWindow.cpp
// 说明：处理文内 .md 链接的点击（由 MarkdownPage::openMarkdown 信号触发）
void MainWindow::handleOpenMarkdownUrl(const QUrl &url)
{
    if (m_currentFilePath.isEmpty()) {
        return;
    }

    const QString rawPath = url.isLocalFile() ? url.toLocalFile() : url.path();
    const QString lower   = rawPath.toLower();
    if (!lower.endsWith(".md") && !lower.endsWith(".markdown")) {
        return;
    }

    QFileInfo curFi(m_currentFilePath);
    QDir baseDir(curFi.absolutePath());
    QUrl baseUrl = QUrl::fromLocalFile(baseDir.absolutePath() + "/");
    QUrl resolved = url;
    if (url.isRelative() || url.scheme().isEmpty()) {
        resolved = baseUrl.resolved(url);
    }

    QString targetPath = resolved.toLocalFile();
    if (targetPath.isEmpty()) {
        targetPath = baseDir.absoluteFilePath(rawPath);
    }

    if (targetPath.isEmpty()) {
        return;
    }

    QFileInfo fi(targetPath);
    if (!fi.exists() || !fi.isFile()) {
        QMessageBox::warning(
            this,
            QStringLiteral("打开失败"),
            QStringLiteral("找不到链接指向的 Markdown 文件：\n%1")
                .arg(targetPath));
        return;
    }

    const QString finalPath = fi.canonicalFilePath().isEmpty()
                                  ? fi.absoluteFilePath()
                                  : fi.canonicalFilePath();
    openMarkdownFile(finalPath);
}

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
// 作用：在 Web 页面中向上/向下滚动一屏高度（约 50%）

void MainWindow::scrollPageUp()    // NEW
{
    if (!m_view) {
        return;
    }
    // ?? JavaScript ????????????? 50%???????????
    const QString js = QStringLiteral(
        R"JS(
(() => {
  const delta = window.innerHeight * 0.5;
  const candidates = [
    document.scrollingElement,
    document.documentElement,
    document.body,
    document.getElementById('md-root'),
    ...document.querySelectorAll('.md-root, .markdown-body')
  ];
  for (const el of candidates) {
    if (!el) continue;
    const maxScroll = el.scrollHeight - el.clientHeight;
    if (maxScroll > 1) {
      el.scrollTop = Math.max(0, el.scrollTop - delta);
      return true;
    }
  }
  window.scrollBy(0, -delta);
  return true;
})();
)JS");
    m_view->page()->runJavaScript(js);
}

void MainWindow::scrollPageDown()  // NEW
{
    if (!m_view) {
        return;
    }
    // ?? JavaScript ????????????? 50%???????????
    const QString js = QStringLiteral(
        R"JS(
(() => {
  const delta = window.innerHeight * 0.5;
  const candidates = [
    document.scrollingElement,
    document.documentElement,
    document.body,
    document.getElementById('md-root'),
    ...document.querySelectorAll('.md-root, .markdown-body')
  ];
  for (const el of candidates) {
    if (!el) continue;
    const maxScroll = el.scrollHeight - el.clientHeight;
    if (maxScroll > 1) {
      el.scrollTop = Math.min(maxScroll, el.scrollTop + delta);
      return true;
    }
  }
  window.scrollBy(0, delta);
  return true;
})();
)JS");
    m_view->page()->runJavaScript(js);
}
// 文件：src/app/MainWindow.cpp
// 作用：在未锁定状态下实现：
//   - 左键拖动整窗
//   - 右键翻页（上半区上一页，下半区下一页）

bool MainWindow::eventFilter(QObject *obj, QEvent *event)   // NEW
{
    // 只关心 Web 内容区域上的鼠标事件
    if (obj == m_view) {
        // 右键翻页
        if (event->type() == QEvent::MouseButtonPress) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::RightButton) {

                if (m_locked) {
                    // 锁定状态下不处理右键翻页（此时窗口本身大多已经穿透）
                    return false;
                }

                const int h = m_view->height();
                const int y = static_cast<int>(me->position().y());
                const bool upperHalf = (y < h / 2);

                if (upperHalf) {
                    scrollPageUp();
                } else {
                    scrollPageDown();
                }

                return true;
            }
        }


        // // 整窗拖动：未锁定时，任意区域按住左键拖动窗口
        // if (event->type() == QEvent::MouseButtonPress) {
        //     auto *me = static_cast<QMouseEvent *>(event);
        //     if (me->button() == Qt::LeftButton) {
        //         // 如果你有 m_locked 标志，在锁定模式下这里直接放行
        //         // if (m_locked) {
        //         //     return false;
        //         // }

        //         // 记录起始拖动位置（相对全局）
        //         m_dragStartPos = me->globalPosition().toPoint();  // 需要在 MainWindow.h 中增加 QPoint m_dragStartPos; // NEW
        //         m_dragging = true;                                // 需要在 MainWindow.h 中增加 bool m_dragging = false; // NEW
        //         return true;
        //     }
        // } else if (event->type() == QEvent::MouseMove) {
        //     auto *me = static_cast<QMouseEvent *>(event);
        //     if (m_dragging) {
        //         const QPoint globalPos = me->globalPosition().toPoint();
        //         const QPoint delta = globalPos - m_dragStartPos;
        //         m_dragStartPos = globalPos;
        //         move(pos() + delta);
        //         return true;
        //     }
        // } else if (event->type() == QEvent::MouseButtonRelease) {
        //     auto *me = static_cast<QMouseEvent *>(event);
        //     if (me->button() == Qt::LeftButton && m_dragging) {
        //         m_dragging = false;
        //         return true;
        //     }
        // }
    }

    // 其他情况交给基类处理
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::moveEvent(QMoveEvent *event)
{
    QMainWindow::moveEvent(event);
    if (m_titleBar) {
        m_titleBar->syncWithMainWindow();
    }
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (m_titleBar) {
        m_titleBar->syncWithMainWindow();
    }
}


// 文件：src/app/MainWindow.cpp
// 读取指定 .md 并渲染显示（当前仍使用 basicMarkdownToHtml 占位渲染）
bool MainWindow::openMarkdownFile(const QString &path, bool addToHistory)
{
    if (!m_view) return false;

    m_openingFile = true;

    QFileInfo fi(path);
    if (!fi.exists() || !fi.isFile()) {
        QMessageBox::warning(
            this,
            QStringLiteral("打开失败"),
            QStringLiteral("找不到文件：\n%1").arg(path));
        m_openingFile = false;
        return false;
    }

    // 统一规范成绝对路径
    const QString absPath = fi.absoluteFilePath();
    m_currentFilePath = absPath;

    // 记住“上次打开目录”，用于下次文件对话框默认目录
    m_lastOpenDir = fi.absolutePath();
    if (!m_lastOpenDir.isEmpty()) {
        QSettings settings(QStringLiteral("zhiz"), QStringLiteral("TransparentMdReader"));
        settings.setValue(QStringLiteral("ui/lastOpenDir"), m_lastOpenDir);
        settings.setValue(QStringLiteral("ui/lastFilePath"), m_currentFilePath);
    }

    QFile file(absPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(
            this,
            QStringLiteral("打开失败"),
            QStringLiteral("无法读取文件：\n%1").arg(absPath));
        m_openingFile = false;
        return false;
    }

    QTextStream in(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    // Qt5 写法
    in.setCodec("UTF-8");
#else
    // Qt6 写法
    in.setEncoding(QStringConverter::Utf8);
#endif
const QString markdown = in.readAll();


    // 更新历史栈（只在 addToHistory = true 时修改栈）
    if (addToHistory) {
        if (m_historyIndex >= 0 && m_historyIndex < m_history.size()) {
            m_history = m_history.mid(0, m_historyIndex + 1);
        }

        if (m_history.isEmpty() || m_history.last() != absPath) {
            m_history.append(absPath);
        }
        m_historyIndex = m_history.size() - 1;
        persistHistory();
    }

    const QUrl baseUrl = QUrl::fromLocalFile(fi.absolutePath() + QLatin1Char('/'));

    // 第一次加载：index.html 还没就绪，先挂起 Markdown 和滚动比例
    if (!m_pageLoaded) {
        const QUrl index = locateIndexPage();

        m_pendingMarkdown     = markdown;
        m_pendingTitle        = fi.fileName();
        m_pendingBaseUrl      = baseUrl.toString();
        m_pendingScrollRatio  = StateDbManager::instance().loadScroll(m_currentFilePath);

        m_view->setUrl(index);
        // m_openingFile 在 loadFinished 回调里统一置回 false
        return true;
    }

        // ???????? Markdown???????
    renderMarkdownInPage(markdown, fi.fileName(), baseUrl);
    setWindowTitle(QStringLiteral("TransparentMdReader - %1").arg(fi.fileName()));

    // ????????
    applyReaderStyle();

    m_lastScrollRatio = StateDbManager::instance().loadScroll(m_currentFilePath);
    if (m_lastScrollRatio > 0.001) {
        applyScrollRatio(m_lastScrollRatio);
    }

    StateDbManager::instance().recordOpen(
        m_currentFilePath,
        fi.lastModified().toSecsSinceEpoch(),
        fi.size());
    rebuildRecentMenu();

    m_openingFile = false;
    return true;
}

void MainWindow::persistHistory()
{
    QSettings settings("zhiz", "TransparentMdReader");
    settings.setValue("history/list", m_history);
    settings.setValue("history/index", m_historyIndex);
    settings.setValue("history/limit", m_historyLimit);
}

void MainWindow::loadHistoryFromSettings()
{
    QSettings settings("zhiz", "TransparentMdReader");
    m_historyLimit = settings.value("history/limit", 20).toInt();
    if (m_historyLimit < 1)
        m_historyLimit = 20;
    if (m_historyLimit > 200)
        m_historyLimit = 200;

    const QStringList list = settings.value("history/list").toStringList();
    int index = settings.value("history/index", -1).toInt();
    m_history = list;
    trimHistory();

    if (m_history.isEmpty()) {
        // 旧版本没有 history/list 的情况：从 SQLite 的最近列表补一份
        const auto recents = StateDbManager::instance().listRecent(m_historyLimit);
        for (const auto &entry : recents) {
            QFileInfo fi(entry.path);
            if (!fi.exists() || !fi.isFile())
                continue;

            const QString finalPath = fi.canonicalFilePath().isEmpty()
                                          ? fi.absoluteFilePath()
                                          : fi.canonicalFilePath();
            m_history.append(finalPath);
        }
        trimHistory();

        if (!m_history.isEmpty()) {
            // 默认指向最新一条
            m_historyIndex = m_history.size() - 1;
            // 同步回 QSettings，后面就直接用 QSettings 的历史
            persistHistory();
        } else {
            m_historyIndex = -1;
        }

        updateNavigationActions();
        return;
    }

    // history/list 非空的情况，按原来的逻辑走
    if (index < 0 || index >= m_history.size()) {
        index = m_history.size() - 1;
    }
    m_historyIndex = index;
    updateNavigationActions();
}


void MainWindow::trimHistory()
{
    if (m_historyLimit < 1) {
        m_historyLimit = 20;
    }
    while (m_history.size() > m_historyLimit) {
        m_history.removeFirst();
        if (m_historyIndex > 0) {
            --m_historyIndex;
        }
    }
    if (m_historyIndex >= m_history.size()) {
        m_historyIndex = m_history.isEmpty() ? -1 : m_history.size() - 1;
    }
}

void MainWindow::applyScrollRatio(double ratio)
{
    if (!m_view || !m_view->page() || ratio <= 0.0) {
        return;
    }

    double clamped = ratio;
    if (clamped > 1.0) {
        clamped = 1.0;
    }

    const QString js = QStringLiteral(
        "(function(r) { "
        "  if (typeof setInitialScroll === 'function') { "
        "    return setInitialScroll(r); "
        "  } "
        "  return false; "
        "})(%1);"
    ).arg(clamped, 0, 'f', 6);

    // ????? m_restoringScroll ? true???????? 0 ??
    const int kMaxAttempts = 5;
    const int kRetryDelayMs = 200;

    std::function<void(int)> applyOnce;
    applyOnce = [this, js, kMaxAttempts, kRetryDelayMs, &applyOnce](int attempt) {
        if (!m_view || !m_view->page()) {
            m_restoringScroll = false;
            return;
        }
        m_view->page()->runJavaScript(js, [this, attempt, kMaxAttempts, kRetryDelayMs, &applyOnce](const QVariant &v) {
            const bool handled = v.toBool();
            if (handled || attempt >= kMaxAttempts) {
                QTimer::singleShot(80, this, [this]() { m_restoringScroll = false; });
            } else {
                QTimer::singleShot(kRetryDelayMs, this, [this, attempt, &applyOnce]() {
                    applyOnce(attempt + 1);
                });
            }
        });
    };

    m_restoringScroll = true;
    applyOnce(0);
}

void MainWindow::autoOpenLastFileIfNeeded()
{
    QSettings settings("zhiz", "TransparentMdReader");
    const bool openLast = settings.value("startup/openLastFile", true).toBool();
    if (!openLast) {
        return;
    }

    const auto recents = StateDbManager::instance().listRecent(10);
    for (const auto &entry : recents) {
        QFileInfo fi(entry.path);
        if (!fi.exists() || !fi.isFile()) {
            StateDbManager::instance().markMissing(entry.path);
            continue;
        }
        const QString finalPath = fi.canonicalFilePath().isEmpty()
                                      ? fi.absoluteFilePath()
                                      : fi.canonicalFilePath();
        // 启动时自动打开最近文件不应破坏已有历史栈，禁用历史截断
        openMarkdownFile(finalPath, /*addToHistory=*/false);
        return;
    }
}

#include "MainWindow.moc"
