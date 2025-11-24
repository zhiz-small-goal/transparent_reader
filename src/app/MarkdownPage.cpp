#include "MarkdownPage.h"

#include <QDesktopServices>

static bool isInternalMarkdown(const QUrl &url)
{
    const QString path = url.path().toLower();
    return path.endsWith(".md") || path.endsWith(".markdown") || path.endsWith(".txt");
}

// NEW: 判断是否为图片链接（本地或远程都按图片处理）
bool isImageUrl(const QUrl &url)  // NEW
{
    const QString path = url.path().toLower();

    return path.endsWith(".png")
        || path.endsWith(".jpg")
        || path.endsWith(".jpeg")
        || path.endsWith(".gif")
        || path.endsWith(".bmp")
        || path.endsWith(".webp")
        || path.endsWith(".svg");
}

bool MarkdownPage::acceptNavigationRequest(const QUrl &url,
                                           NavigationType type,
                                           bool isMainFrame)
{
    Q_UNUSED(isMainFrame);

    if (type == QWebEnginePage::NavigationTypeLinkClicked) {
        // 1) 内部 Markdown 链接：交给 MainWindow 处理
        if (isInternalMarkdown(url)) {
            emit openMarkdown(url);
            return false; // 阻止 WebEngine 自己跳转到 .md
        }

        // 2) 图片链接：交给 MainWindow 打开图片浮层           // NEW
        if (isImageUrl(url)) {                                      // NEW
            emit openImage(url);                                    // NEW
            return false;                                           // NEW
        }

        // 3) 其它（http/https、本地 html、pdf 等）：交给系统默认程序          // CHANGED
        QDesktopServices::openUrl(url);
        return false;
    }

    return QWebEnginePage::acceptNavigationRequest(url, type, isMainFrame);
}

QWebEnginePage *MarkdownPage::createWindow(QWebEnginePage::WebWindowType type)
{
    Q_UNUSED(type);
    // 所有 “新窗口” 请求（target="_blank"、Ctrl+点击 等）都在当前页面中处理，
    // 这样会继续走 acceptNavigationRequest() 逻辑，由我们统一分发：
    //  - 内部 .md 链接 -> emit openMarkdown(url)
    //  - 图片链接      -> emit openImage(url)
    //  - 其它链接      -> QDesktopServices::openUrl(url)
    return this;
}
