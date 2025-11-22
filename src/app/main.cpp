#include "MainWindow.h"

#include <QApplication>
#include <QCoreApplication>

int main(int argc, char *argv[])
{
    QCoreApplication::setOrganizationName(QStringLiteral("zhiz"));
    QCoreApplication::setApplicationName(QStringLiteral("TransparentMdReader"));
    QApplication app(argc, argv);
    MainWindow window;
    window.show();
    return app.exec();
}
