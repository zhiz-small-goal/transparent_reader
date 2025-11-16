#include "MainWindow.h"

#include <QCoreApplication>
#include <QDir>
#include <QUrl>
#include <QString>
#include <QWebEngineView>

namespace
{
QUrl locateIndexPage()
{
    QDir dir(QCoreApplication::applicationDirPath());
    constexpr int kMaxLevels = 8;
    for (int i = 0; i < kMaxLevels; ++i) {
        if (dir.exists("resources/web/index.html")) {
            return QUrl::fromLocalFile(dir.absoluteFilePath("resources/web/index.html"));
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
    <h1>Hello, TransparentMdReader</h1>
    <p>TODO: hook up Markdown rendering / web channel / state persistence.</p>
  </body>
</html>)");
}
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_view(new QWebEngineView(this))
{
    //set default window size 
    resize(600, 450);

    setMinimumSize(300, 400);

    setCentralWidget(m_view);

    const QUrl pageUrl = locateIndexPage();
    if (pageUrl.isValid()) {
        m_view->load(pageUrl);
    } else {
        m_view->setHtml(placeholderHtml());
    }
}

MainWindow::~MainWindow() = default;
