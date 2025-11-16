#include "MarkdownPage.h"

#include <QDesktopServices>
#include <QFileInfo>

static bool isInternalMarkdown(const QUrl &url)
{
    // 只关心相对路径或本地文件，并且后缀是 .md / .markdown
    const QString path = url.path();
    if (path.isEmpty())
        return false;

    const QString lower = path.toLower();
    return lower.endsWith(".md") || lower.endsWith(".markdown");
}

bool MarkdownPage::acceptNavigationRequest(const QUrl &url,
                                           NavigationType type,
                                           bool isMainFrame)
{
    Q_UNUSED(isMainFrame);

    if (type == QWebEnginePage::NavigationTypeLinkClicked) {
        if (isInternalMarkdown(url)) {
            // 内部 Markdown 链接：交给 MainWindow 处理
            emit openMarkdown(url);
        } else {
            // 其它链接：用系统默认浏览器打开（Chrome / Edge 等）
            QDesktopServices::openUrl(url);
        }
        // 自己处理，不让 WebEngine 再去加载
        return false;
    }

    // 不是点击链接（比如初次加载 index.html），走默认逻辑
    return QWebEnginePage::acceptNavigationRequest(url, type, isMainFrame);
}
