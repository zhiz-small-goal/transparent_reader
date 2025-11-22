// 文件：src/app/StateDbManager.h
#ifndef TRANSPARENTMDREADER_STATEDBMANAGER_H
#define TRANSPARENTMDREADER_STATEDBMANAGER_H

#include <QString>
#include <QVector>

class StateDbManager
{
public:
    struct RecentEntry {
        QString path;
        double  scrollRatio = 0.0;
        qint64  lastOpenTime = 0;
        bool    pinned = false;
    };

    static StateDbManager &instance();

    // 打开或创建数据库；dbPath 为空时自动使用 AppDataLocation/state.db
    bool open(const QString &dbPath = QString());

    // 记录一次“打开文件”：更新 last_open_time / 文件大小 / mtime，如不存在则插入
    bool recordOpen(const QString &path, qint64 fileMtime, qint64 fileSize);

    // 更新滚动进度（0.0~1.0），如不存在则插入一行
    bool updateScroll(const QString &path, double ratio);

    // 读取滚动进度，不存在时返回 0.0
    double loadScroll(const QString &path) const;

    // 最近文件列表（按 pin DESC, last_open_time DESC 排序）
    QVector<RecentEntry> listRecent(int limit = 10) const;

private:
    StateDbManager() = default;
    bool ensureSchema();
    QString normalizePath(const QString &path) const;
    bool executePragmas();
    qint64 currentTimestamp() const;

    bool m_ready = false;
};

#endif // TRANSPARENTMDREADER_STATEDBMANAGER_H
