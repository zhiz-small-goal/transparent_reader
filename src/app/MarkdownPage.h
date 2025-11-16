#ifndef TRANSPARENTMDREADER_MARKDOWNPAGE_H
#define TRANSPARENTMDREADER_MARKDOWNPAGE_H

#include <QWebEnginePage>
#include <QUrl>

class MarkdownPage : public QWebEnginePage
{
    Q_OBJECT
public:
    using QWebEnginePage::QWebEnginePage;

signals:
    // 发现内部 .md 链接时，让 MainWindow 处理
    void openMarkdown(const QUrl &url);

protected:
    bool acceptNavigationRequest(const QUrl &url,
                                 NavigationType type,
                                 bool isMainFrame) override;
};

#endif // TRANSPARENTMDREADER_MARKDOWNPAGE_H
