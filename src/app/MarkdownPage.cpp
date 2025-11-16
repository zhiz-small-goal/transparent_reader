#include "MarkdownPage.h"

#include <QDesktopServices>

static bool isInternalMarkdown(const QUrl &url)
{
    const QString path = url.path().toLower();
    return path.endsWith(".md") || path.endsWith(".markdown");
}

bool MarkdownPage::acceptNavigationRequest(const QUrl &url,
                                           NavigationType type,
                                           bool isMainFrame)
{
    Q_UNUSED(isMainFrame);

    if (type == QWebEnginePage::NavigationTypeLinkClicked) {
        if (isInternalMarkdown(url)) {
            // 内部 md 链接 → 交给 MainWindow
            emit openMarkdown(url);
        } else {
            // 其它（http / https 等） → 交给系统默认浏览器
            QDesktopServices::openUrl(url);
        }
        return false; // 阻止 WebEngine 自己跳转
    }

    return QWebEnginePage::acceptNavigationRequest(url, type, isMainFrame);
}
