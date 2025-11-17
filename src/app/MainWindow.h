#ifndef TRANSPARENTMDREADER_MAINWINDOW_H
#define TRANSPARENTMDREADER_MAINWINDOW_H

#include <QMainWindow>
#include <QString>
#include <QUrl>

QT_BEGIN_NAMESPACE
class QWebEngineView;
QT_END_NAMESPACE

class ImageOverlay;                    // NEW 前向声明

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    // 打开指定 Markdown 文件
    void openMarkdownFile(const QString &path);

private slots:
    // Ctrl+O 打开文件对话框
    void openMarkdownFileFromDialog();
    

    // Web 页面里点击内部 .md 链接时调用
    void handleOpenMarkdownUrl(const QUrl &url);

    void handleOpenImageUrl(const QUrl &url); 

private:
    // 把 markdown 文本送进 WebEngine（以后切到 marked.js 也会用）
    void renderMarkdownInPage(const QString &markdown,
                              const QString &title,
                              const QUrl    &baseUrl);

private:
    QWebEngineView *m_view = nullptr;
    ImageOverlay   *m_imageOverlay = nullptr;

    QString m_lastOpenDir;        // 最近打开目录
    QString m_currentFilePath;    // 当前 md 文件绝对路径

    bool    m_useEmbeddedViewer = false;
    bool    m_pageLoaded        = false;
    QString m_pendingMarkdown;
    QString m_pendingTitle;
    QString m_pendingBaseUrl;     // NEW：待渲染文档的 baseUrl
};

#endif // TRANSPARENTMDREADER_MAINWINDOW_H