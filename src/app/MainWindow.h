#ifndef TRANSPARENTMDREADER_MAINWINDOW_H
#define TRANSPARENTMDREADER_MAINWINDOW_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
class QWebEngineView;
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    QWebEngineView *m_view = nullptr;
};

#endif // TRANSPARENTMDREADER_MAINWINDOW_H
