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

    // 打开指定路径的 Markdown 文件（给以后其它入口复用）
    void openMarkdownFile(const QString &path);


private:
    // 弹出文件选择框，选择 .md，内部调用 openMarkdownFile()
    void openMarkdownFileFromDialog();


private:
    QWebEngineView *m_view = nullptr;
};

#endif // TRANSPARENTMDREADER_MAINWINDOW_H
