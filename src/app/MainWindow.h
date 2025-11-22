// 文件：src/app/MainWindow.h
#ifndef TRANSPARENTMDREADER_MAINWINDOW_H
#define TRANSPARENTMDREADER_MAINWINDOW_H

#include <QMainWindow>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QByteArray>          // NEW: nativeEvent 所需
#include <QtCore/qglobal.h>
#include <QEvent>
#include <QSystemTrayIcon>
#include <QTimer>

QT_BEGIN_NAMESPACE
class QAction;
class QDragEnterEvent;
class QDropEvent;
class QWebEngineView;
class QMenu;
QT_END_NAMESPACE

class ImageOverlay;
class TitleBar;                        // NEW 前向声明

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    // 打开指定 Markdown 文件（返回是否成功；默认写入历史）
    bool openMarkdownFile(const QString &path, bool addToHistory = true);

    // 锁定 / 解锁（锁定 = 内容穿透）
    bool isLocked() const noexcept { return m_locked; }
    void setLocked(bool locked);

    // 用户通过标题栏点击 🔒 时切换锁定偏好
    void toggleLockByUser();

    // 由标题栏翻页按钮调用：滚动一屏（向上/向下）
    void scrollPageUp();    // PageUp
    void scrollPageDown();  // PageDown

    // 打开“阅读设置”对话框（非模态，实时预览）
    void openSettingsDialog();


 public slots:
    void goBack();
    void goForward();   


protected:
// 拦截 WebEngine 区域的鼠标事件（右键翻页 / 整窗拖动）
bool eventFilter(QObject *obj, QEvent *event) override; // NEW

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

    // Windows 下用于实现“内容区域点击穿透”的 native 事件处理
    bool nativeEvent(const QByteArray &eventType,
                     void *message,
                     qintptr *result) override;
    
    // 用于同步浮动按钮条（Settings / X）的几何位置
    void moveEvent(QMoveEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void openMarkdownFileFromDialog();
    void handleOpenMarkdownUrl(const QUrl &url);
    void handleOpenImageUrl(const QUrl &url);
    void showContextMenu(const QPoint &pos);
    void handleTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void toggleAutoStart(bool enabled);
    void toggleLogging(bool enabled);
    void quitFromTray();

private:
    void renderMarkdownInPage(const QString &markdown,
                              const QString &title,
                              const QUrl    &baseUrl);
    bool canGoBack() const;
    bool canGoForward() const;
    void updateNavigationActions();
    void updateClickThroughState();
    void createSystemTray();
    void updateTrayChecks();
    void applyScrollRatio(double ratio);

private:
    QWebEngineView *m_view         = nullptr;
    ImageOverlay   *m_imageOverlay = nullptr;
    TitleBar       *m_titleBar     = nullptr;
    QSystemTrayIcon *m_trayIcon    = nullptr;
    QMenu           *m_trayMenu    = nullptr;
    QAction         *m_trayOpenAction      = nullptr;
    QAction         *m_trayAutoStartAction = nullptr;
    QAction         *m_trayLoggingAction   = nullptr;
    QAction         *m_trayQuitAction      = nullptr;

    QString     m_lastOpenDir;
    QString     m_currentFilePath;
    QStringList m_history;
    int         m_historyIndex = -1;
    QAction    *m_backAction    = nullptr;
    QAction    *m_forwardAction = nullptr;

    bool    m_useEmbeddedViewer = false;
    bool    m_pageLoaded        = false;
    QString m_pendingMarkdown;
    QString m_pendingTitle;
    QString m_pendingBaseUrl;
    QTimer *m_scrollTimer       = nullptr;
    double  m_lastScrollRatio   = 0.0;

        // ===== 整窗拖动状态（未锁定时用） =====  // NEW
    bool   m_dragging      = false;
    QPoint m_dragStartPos;

    // 应用当前阅读样式（字体 / 颜色 / 背景）到当前页面
    void applyReaderStyle();

    // 当前是否“实际处于锁定（穿透）”状态
    bool m_locked = false;

    // 用户的基础锁定偏好：
    //   true  = 平时保持锁定（默认值）
    //   false = 平时保持解锁
    // Ctrl 始终可以临时解锁，但不会修改这个值
    bool m_manualLocked = true;
    bool m_loggingEnabled   = false;
    bool m_autoStartEnabled = false;

};

#endif // TRANSPARENTMDREADER_MAINWINDOW_H
