#ifndef TRANSPARENTMDREADER_MAINWINDOW_H
#define TRANSPARENTMDREADER_MAINWINDOW_H

#include <QMainWindow>
#include <QString>
#include <QStringList>
#include <QUrl>

QT_BEGIN_NAMESPACE
class QAction;
class QDragEnterEvent;
class QDropEvent;
class QWebEngineView;
QT_END_NAMESPACE

class ImageOverlay;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    bool openMarkdownFile(const QString &path, bool addToHistory = true);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void openMarkdownFileFromDialog();
    void handleOpenMarkdownUrl(const QUrl &url);
    void handleOpenImageUrl(const QUrl &url);
    void goBack();
    void goForward();
    void showContextMenu(const QPoint &pos);

private:
    void renderMarkdownInPage(const QString &markdown,
                              const QString &title,
                              const QUrl    &baseUrl);
    bool canGoBack() const;
    bool canGoForward() const;
    void updateNavigationActions();

private:
    QWebEngineView *m_view = nullptr;
    ImageOverlay   *m_imageOverlay = nullptr;

    QString m_lastOpenDir;
    QString m_currentFilePath;
    QStringList m_history;
    int m_historyIndex = -1;
    QAction *m_backAction = nullptr;
    QAction *m_forwardAction = nullptr;

    bool    m_useEmbeddedViewer = false;
    bool    m_pageLoaded        = false;
    QString m_pendingMarkdown;
    QString m_pendingTitle;
    QString m_pendingBaseUrl;
};

#endif // TRANSPARENTMDREADER_MAINWINDOW_H
