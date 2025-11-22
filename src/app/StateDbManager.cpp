#include "StateDbManager.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QStandardPaths>
#include <QVariant>

namespace {

QString defaultDatabasePath()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dir.isEmpty()) {
        dir = QDir::homePath() + QStringLiteral("/.transparentmdreader");
    }
    QDir().mkpath(dir);
    return dir + QStringLiteral("/state.db");
}

bool runQuery(QSqlQuery &query, const QString &sql)
{
    if (!query.exec(sql)) {
        qWarning("SQLite exec failed: %s (%s)", qPrintable(sql), qPrintable(query.lastError().text()));
        return false;
    }
    return true;
}

bool runPrepared(QSqlQuery &query)
{
    if (!query.exec()) {
        qWarning("SQLite exec failed: %s", qPrintable(query.lastError().text()));
        return false;
    }
    return true;
}

} // namespace

StateDbManager &StateDbManager::instance()
{
    static StateDbManager inst;
    return inst;
}

bool StateDbManager::open(const QString &dbPath)
{
    if (m_ready) {
        return true;
    }

    QString path = dbPath;
    if (path.isEmpty()) {
        path = defaultDatabasePath();
    }

    if (!QSqlDatabase::contains(QStringLiteral("state"))) {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("state"));
        db.setDatabaseName(path);
        if (!db.open()) {
            qWarning("Failed to open state.db: %s", qPrintable(db.lastError().text()));
            return false;
        }
    }

    if (!executePragmas()) {
        return false;
    }

    if (!ensureSchema()) {
        return false;
    }

    m_ready = true;
    return true;
}

bool StateDbManager::ensureSchema()
{
    QSqlDatabase db = QSqlDatabase::database(QStringLiteral("state"));
    if (!db.isOpen()) {
        return false;
    }

    QSqlQuery query(db);

    if (!runQuery(query, QStringLiteral(
                      "CREATE TABLE IF NOT EXISTS app_meta ("
                      " version INTEGER NOT NULL"
                      ");"))) {
        return false;
    }

    if (!runQuery(query, QStringLiteral(
                      "CREATE TABLE IF NOT EXISTS documents ("
                      " id INTEGER PRIMARY KEY,"
                      " path TEXT NOT NULL UNIQUE,"
                      " scroll_ratio REAL NOT NULL DEFAULT 0.0,"
                      " last_open_time INTEGER NOT NULL DEFAULT 0,"
                      " file_mtime INTEGER NOT NULL DEFAULT 0,"
                      " last_file_size INTEGER NOT NULL DEFAULT 0,"
                      " pin INTEGER NOT NULL DEFAULT 0"
                      ");"))) {
        return false;
    }

    if (!runQuery(query, QStringLiteral(
                      "CREATE INDEX IF NOT EXISTS idx_documents_pin_last_open "
                      "ON documents(pin DESC, last_open_time DESC);"))) {
        return false;
    }

    // 初始化 version
    if (!runQuery(query, QStringLiteral("INSERT OR IGNORE INTO app_meta(version) VALUES(1);"))) {
        return false;
    }

    return true;
}

QString StateDbManager::normalizePath(const QString &path) const
{
    QFileInfo fi(path);
    if (!fi.exists()) {
        return fi.absoluteFilePath();
    }
    return fi.canonicalFilePath();
}

qint64 StateDbManager::currentTimestamp() const
{
    return QDateTime::currentSecsSinceEpoch();
}

bool StateDbManager::recordOpen(const QString &path, qint64 fileMtime, qint64 fileSize)
{
    if (!open()) {
        return false;
    }
    const QString norm = normalizePath(path);
    QSqlDatabase db = QSqlDatabase::database(QStringLiteral("state"));
    QSqlQuery query(db);

    if (!db.transaction()) {
        qWarning("Failed to start transaction: %s", qPrintable(db.lastError().text()));
    }

    query.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO documents(path, file_mtime, last_file_size, last_open_time) "
        "VALUES(:path, :mtime, :size, :ts);"));
    query.bindValue(QStringLiteral(":path"), norm);
    query.bindValue(QStringLiteral(":mtime"), fileMtime);
    query.bindValue(QStringLiteral(":size"), fileSize);
    query.bindValue(QStringLiteral(":ts"), currentTimestamp());
    if (!runPrepared(query)) {
        db.rollback();
        return false;
    }

    query.clear();
    query.prepare(QStringLiteral(
        "UPDATE documents "
        "SET last_open_time=:ts, file_mtime=:mtime, last_file_size=:size "
        "WHERE path=:path;"));
    query.bindValue(QStringLiteral(":ts"), currentTimestamp());
    query.bindValue(QStringLiteral(":mtime"), fileMtime);
    query.bindValue(QStringLiteral(":size"), fileSize);
    query.bindValue(QStringLiteral(":path"), norm);
    if (!runPrepared(query)) {
        db.rollback();
        return false;
    }

    db.commit();
    return true;
}

bool StateDbManager::updateScroll(const QString &path, double ratio)
{
    if (!open()) {
        return false;
    }
    double clamped = ratio;
    if (clamped < 0.0) clamped = 0.0;
    if (clamped > 1.0) clamped = 1.0;

    const QString norm = normalizePath(path);
    QSqlDatabase db = QSqlDatabase::database(QStringLiteral("state"));
    QSqlQuery query(db);

    query.prepare(QStringLiteral(
        "INSERT INTO documents(path, scroll_ratio, last_open_time) "
        "VALUES(:path, :ratio, :ts) "
        "ON CONFLICT(path) DO UPDATE SET scroll_ratio=excluded.scroll_ratio;"));
    query.bindValue(QStringLiteral(":path"), norm);
    query.bindValue(QStringLiteral(":ratio"), clamped);
    query.bindValue(QStringLiteral(":ts"), currentTimestamp());
    return runPrepared(query);
}

double StateDbManager::loadScroll(const QString &path) const
{
    if (!m_ready && !const_cast<StateDbManager *>(this)->open()) {
        return 0.0;
    }

    const QString norm = normalizePath(path);
    QSqlDatabase db = QSqlDatabase::database(QStringLiteral("state"));
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT scroll_ratio FROM documents WHERE path=:path;"));
    query.bindValue(QStringLiteral(":path"), norm);
    if (!runPrepared(query)) {
        return 0.0;
    }
    if (query.next()) {
        return query.value(0).toDouble();
    }
    return 0.0;
}

QVector<StateDbManager::RecentEntry> StateDbManager::listRecent(int limit) const
{
    QVector<RecentEntry> result;
    if (!m_ready && !const_cast<StateDbManager *>(this)->open()) {
        return result;
    }
    if (limit <= 0) {
        return result;
    }

    QSqlDatabase db = QSqlDatabase::database(QStringLiteral("state"));
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT path, scroll_ratio, last_open_time, pin "
        "FROM documents "
        "WHERE last_open_time > 0 "
        "ORDER BY pin DESC, last_open_time DESC "
        "LIMIT :limit;"));
    query.bindValue(QStringLiteral(":limit"), limit);
    if (!runPrepared(query)) {
        return result;
    }

    while (query.next()) {
        RecentEntry e;
        e.path = query.value(0).toString();
        e.scrollRatio = query.value(1).toDouble();
        e.lastOpenTime = query.value(2).toLongLong();
        e.pinned = query.value(3).toInt() != 0;
        result.append(e);
    }
    return result;
}

bool StateDbManager::executePragmas()
{
    QSqlDatabase db = QSqlDatabase::database(QStringLiteral("state"));
    if (!db.isOpen()) {
        return false;
    }
    QSqlQuery query(db);
    return runQuery(query, QStringLiteral("PRAGMA journal_mode=WAL;"))
        && runQuery(query, QStringLiteral("PRAGMA synchronous=NORMAL;"))
        && runQuery(query, QStringLiteral("PRAGMA foreign_keys=ON;"))
        && runQuery(query, QStringLiteral("PRAGMA busy_timeout=250;"));
}
