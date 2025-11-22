# TransparentMdReader 状态与历史记录设计（SQLite 方案）

> 面向 TransparentMdReader 的「记忆模块」设计文档，用于指导后续 C++17 / Qt6 实现。

---

## 1. 目标与职责划分

### 1.1 设计目标

本方案希望解决：

- 记住用户最近打开的 Markdown / HTML 文件；
- 记住每个文件的阅读进度（滚动位置）；
- 记住与文件相关的一些元信息（最后打开时间、文件大小、修改时间、是否置顶）；
- 在应用升级、路径变化时保持较好的兼容性；
- 不干扰现有的 UI 配置（窗口大小、主题、透明度等）。

同时满足：

- 依赖 Qt 官方推荐的路径和 SQLite 使用方式，保证跨平台和可维护性；
- 单实例应用，单连接访问数据库，避免多线程并发问题；
- 支持将来平滑增加字段或新增表（schema 迁移）。

### 1.2 职责划分：QSettings vs SQLite

按照职责划分：

- **QSettings**：继续负责「配置类」信息  
  - 窗口大小/位置、是否置顶；
  - 主题（浅色/深色）、背景颜色与透明度；
  - 是否显示右侧滚动条、字体大小、字体颜色等阅读样式；
  - 是否开机自启动等。

- **SQLite（state.db）**：负责「文件级状态」与「历史记录」  
  - 最近打开的文件列表；
  - 每个文件的 `scroll_ratio`（0.0–1.0 的阅读进度）；
  - 文件元信息（最后打开时间、文件大小、文件修改时间）；
  - 文件是否被置顶（pin）。

这样可以避免重复造轮子，又能把结构化、可查询的历史记录交给数据库处理。

---

## 2. 数据库存放位置与连接管理

### 2.1 存放位置

使用 Qt 官方推荐的标准路径：

- 位置：`QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)`  
- 数据库文件名：`state.db`  
- 最终路径形如：

- Windows：`C:/Users/<User>/AppData/Roaming/<OrgName>/<AppName>/state.db`  
- 其他平台：对应的应用数据目录下的 `state.db`

注意：在使用 `QStandardPaths` 前，需要先设置：

```cpp
QCoreApplication::setOrganizationName("YourOrg");
QCoreApplication::setApplicationName("TransparentMdReader");
```

再通过：

```cpp
QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
QDir().mkpath(dir);
QString dbPath = dir + "/state.db";
```

来获得最终文件路径并确保目录存在。

### 2.2 QSqlDatabase 连接策略

- 使用 Qt 内置的 `QSQLITE` 驱动。  
- 采用**单实例 + 单连接 + 主线程访问**策略：
  - 在主线程创建 `QSqlDatabase` 连接（connectionName 建议固定为 `"state"`）；
  - 所有数据库操作在主线程执行；
  - 若将来需要后台线程操作数据库，再为每个线程单独创建连接。

原因：

- Qt 文档明确要求：`QSqlDatabase` 实例只能在创建它的线程中访问；
- 对于 TransparentMdReader 这种桌面小工具，目前主线程写入负载很小，完全足够。

---

## 3. Schema 设计（首版）

数据库仅负责状态和历史记录，首版设计尽量保持简单：**只建一个核心表 `documents`**。

### 3.1 documents 表

```sql
CREATE TABLE IF NOT EXISTS documents (
    id              INTEGER PRIMARY KEY,
    path            TEXT NOT NULL UNIQUE,
    scroll_ratio    REAL NOT NULL DEFAULT 0.0,  -- 0.0 ~ 1.0
    last_open_time  INTEGER NOT NULL DEFAULT 0, -- Unix 时间戳（秒），0 表示从未打开
    file_mtime      INTEGER NOT NULL DEFAULT 0, -- 文件 mtime（秒）
    last_file_size  INTEGER NOT NULL DEFAULT 0, -- 字节数
    pin             INTEGER NOT NULL DEFAULT 0  -- 0: 未置顶, 1: 置顶
);
```

说明：

- `path`：**规范化后的绝对路径**，并设置为 `UNIQUE`，保证同一文件只出现一行；
- `scroll_ratio`：页面纵向滚动位置，以 0.0–1.0 的比例存储，便于在不同窗口尺寸下恢复；
- `last_open_time`：最后一次打开时间，Unix 时间戳（秒）；
- `file_mtime` / `last_file_size`：用于检测文件是否变化（可选）；
- `pin`：置顶标记，0/1。

### 3.2 索引

为了高效获取「最近文件」，同时支持置顶排序，建议建立多列索引：

```sql
CREATE INDEX IF NOT EXISTS idx_documents_pin_last_open_time
ON documents(pin DESC, last_open_time DESC);
```

这样即可直接按：

```sql
SELECT path, scroll_ratio, last_open_time, pin
FROM documents
WHERE last_open_time > 0
ORDER BY pin DESC, last_open_time DESC
LIMIT :count;
```

得到置顶优先 + 最近优先的文件列表。

### 3.3 关于 recents 表

原始方案中有一个单独的 `recents` 表：

```sql
recents(id INTEGER PRIMARY KEY AUTOINCREMENT,
        doc_id INTEGER NOT NULL,
        opened_at INTEGER NOT NULL,
        UNIQUE(doc_id));
```

在只需要「最近一次打开时间」的情况下，这张表与 `documents.last_open_time` 信息重复。  
因此首版设计中可以**不建 `recents` 表**，只使用 `documents.last_open_time` 即可。

如果将来需要「打开历史时间线」（一个文档多次打开的记录），再单独引入：

```sql
open_events (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    doc_id     INTEGER NOT NULL,
    opened_at  INTEGER NOT NULL,
    FOREIGN KEY(doc_id) REFERENCES documents(id) ON DELETE CASCADE
);
```

目前先保持 schema 简单。

---

## 4. 路径规范化策略

为避免同一文件因为路径大小写、相对路径等问题导致多条记录，需要在写入前做统一规范化：

```cpp
QString normalizePath(const QString &rawPath)
{
    QFileInfo fi(rawPath);
    QString canonical = fi.canonicalFilePath(); // 解析 ..、符号链接 等
#ifdef Q_OS_WIN
    canonical = canonical.toLower();           // Windows 下一般视为大小写不敏感
#endif
    return canonical;
}
```

后续所有与数据库交互的接口都应以 `normalizePath()` 的结果作为唯一 key。

---

## 5. 版本管理与初始化

### 5.1 版本字段选择

有两种常见方式：

1. 维护专门的 `app_meta` 表；
2. 使用 SQLite 内置的 `PRAGMA user_version`。

这里建议直接采用 `user_version`：

- SQLite 自身不会使用 `user_version`，完全留给应用；
- 读写简单：

  ```sql
  PRAGMA user_version;         -- 读取
  PRAGMA user_version = 1;     -- 设置
  ```

约定：

- `user_version = 0`：数据库未初始化；
- `user_version = 1`：首版 schema（只有 documents 表）。

### 5.2 初始化流程

应用启动时（或首次需要访问 state.db 时）：

1. 拼接出 `dbPath`，创建目录；
2. 打开 `QSqlDatabase` 连接，设置 PRAGMA：

   ```cpp
   QSqlQuery pragma(db);
   pragma.exec("PRAGMA journal_mode=WAL;");
   pragma.exec("PRAGMA synchronous=NORMAL;");
   pragma.exec("PRAGMA foreign_keys=ON;");
   pragma.exec("PRAGMA busy_timeout=250;");
   ```

3. 查询 `PRAGMA user_version;`：
   - 若为 0：开启事务，创建所有表和索引，设置 `user_version = 1` 后提交；
   - 若为 1：直接返回；
   - 若大于 1：按后续版本的迁移逻辑处理。

> 注：`journal_mode=WAL` + `synchronous=NORMAL` 是 SQLite 官方推荐的常用配置之一：在桌面应用中可以改善写入性能，同时在崩溃场景下最多丢失最近一次事务，适合作为本地状态存储。

---

## 6. 典型读写流程

### 6.1 打开文件

当用户在 UI 中打开一个文件时：

1. 规范化路径：`normPath = normalizePath(rawPath);`
2. 获取文件当前的 `mtime` 和 `size`：

   ```cpp
   QFileInfo fi(normPath);
   qint64 mtimeSec = fi.lastModified().toSecsSinceEpoch();
   qint64 size     = fi.size();
   qint64 nowSec   = QDateTime::currentSecsSinceEpoch();
   ```

3. 在事务中执行「插入或更新」：

   ```sql
   INSERT INTO documents(path, file_mtime, last_file_size, last_open_time)
   VALUES (:path, :mtime, :size, :now)
   ON CONFLICT(path) DO UPDATE SET
       last_open_time = excluded.last_open_time,
       file_mtime     = excluded.file_mtime,
       last_file_size = excluded.last_file_size;
   ```

4. 随后（或在加载完成后）读取该文档的 `scroll_ratio`：

   ```sql
   SELECT scroll_ratio
   FROM documents
   WHERE path = :path;
   ```

   若没有找到（首次插入），默认 `0.0`。

5. 将 `scroll_ratio` 应用到前端 WebView（通过 JavaScript 滚动）。

### 6.2 滚动位置更新（节流）

- 前端每次滚动时，可以计算当前滚动比例（0–1）并通过信号通知后端；
- 后端只更新内存中的缓存结构 `QHash<QString, double>`；
- 使用一个 `QTimer`（如 500ms）周期性地：
  - 找出最近有变化的条目；
  - 批量执行：

    ```sql
    UPDATE documents
    SET scroll_ratio = :ratio
    WHERE path = :path;
    ```

- 写入可以无需特别包事务（单条 UPDATE 也会隐式事务），如果一次要更新多条，也可以人为包一层 `BEGIN IMMEDIATE ... COMMIT`。

### 6.3 获取最近文档列表

用于：

- 主窗口「最近打开」列表；
- 标题栏「上一篇/下一篇」按钮的数据来源。

典型查询：

```sql
SELECT path, scroll_ratio, last_open_time, pin
FROM documents
WHERE last_open_time > 0
ORDER BY pin DESC, last_open_time DESC
LIMIT :count;
```

UI 层可以把结果放到一个 `QVector<DocumentEntry>`，并维护当前 index，实现：

- 上一篇：`currentIndex - 1`；
- 下一篇：`currentIndex + 1`。

### 6.4 置顶 / 取消置顶

- 置顶：`UPDATE documents SET pin = 1 WHERE path = :path;`
- 取消置顶：`UPDATE documents SET pin = 0 WHERE path = :path;`

置顶状态只影响排序，不影响其他逻辑。

### 6.5 清空历史

提供一个 UI 操作「清空阅读历史」：

- 仅清除阅读状态，不删除文件本身；
- 简单实现：

```sql
UPDATE documents
SET scroll_ratio   = 0.0,
    last_open_time = 0,
    pin            = 0;
```

或进一步提供「删除所有记录」按钮：

```sql
DELETE FROM documents;
```

---

## 7. Qt 集成与线程注意事项

### 7.1 单连接、主线程访问

- 在主线程创建 `QSqlDatabase`：

  ```cpp
  QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "state");
  db.setDatabaseName(dbPath);
  db.open();
  ```

- 将 `db` 包装在一个 `StateDbManager` 单例/成员中，暴露线程安全的高层接口：
  - `openOrCreate();`
  - `recordOpen(path);`
  - `updateScroll(path, ratio);`
  - `loadLastScroll(path);`
  - `listRecent(limit);`
- 保证这些调用都在主线程执行（可以在调试模式下加 `Q_ASSERT(QThread::currentThread() == qApp->thread());`）。

### 7.2 将来如需放到后台线程

如果发现数据库操作阻塞 UI，可以考虑：

- 新建一个专门的 QThread（或线程池中的单线程），在该线程内创建自己的 `QSqlDatabase` 连接；
- 提供信号/队列，将 UI 的操作请求转发到该线程执行；
- 遵守 Qt 要求：一个连接只在创建它的线程中使用，每个线程用独立的 connectionName。

TransparentMdReader 当前工作量很小，首版可以完全在主线程处理。

---

## 8. 性能与可靠性设置

### 8.1 推荐 PRAGMA

首版建议在打开数据库之后执行一次：

```sql
PRAGMA journal_mode = WAL;
PRAGMA synchronous = NORMAL;
PRAGMA foreign_keys = ON;
PRAGMA busy_timeout = 250;
```

含义概述：

- `journal_mode = WAL`：启用 Write-Ahead Logging，改善并发读写与整体性能；
- `synchronous = NORMAL`：减少 fsync 次数，在桌面状态类数据场景中安全性足够；
- `foreign_keys = ON`：若以后引入其他表（如 open_events），可以可靠地使用外键约束；
- `busy_timeout = 250`：在数据库被锁时最多等待 250ms，避免立即返回「database is locked」。

### 8.2 数据库大小控制

- 每次启动时可检查 `state.db` 文件大小，如超过一定阈值（如 50MB）提醒用户清理；
- 提供 UI 操作「压缩数据库」：执行 `VACUUM;`（注意这一操作会阻塞一段时间，适合在用户手动触发时执行）。

目前方案中 `documents` 表的数据量不会太大，正常使用下数据库尺寸通常在几 MB 以内。

---

## 9. 后续扩展方向

在首版稳定之后，可以考虑：

1. **打开历史时间线**
   - 新增 `open_events` 表，记录每次打开的时间点；
   - 用于统计使用习惯、最近 N 次打开等高级功能。

2. **搜索与标签**
   - 为 `documents` 增加 `title`、`tags` 字段；
   - 支持按文件名/路径片段模糊搜索、按标签筛选。

3. **跨设备同步（远期）**
   - 将 `documents` 简化为可序列化 JSON，再通过云存储进行同步；
   - 本地仍然使用 SQLite 提供索引与查询能力。

4. **调试工具**
   - 在设置中加入「导出状态 DB」功能，方便调试问题；
   - 提供一个简单的「内部状态查看」窗口（只读列表）。

---

## 10. 程序启动时自动打开上次文件

### 10.1 行为定义

- 默认行为：启动程序时，如果存在记录中的“最近一次打开的文件”，并且该文件仍然存在，则自动打开该文件并恢复滚动位置。
- 例外情况：
  - 若 state.db 不存在或无法打开：跳过自动打开，进入“空窗口”状态。
  - 若最近文件路径已失效（文件被删除 / 移动）：从 documents 表中删除该记录（或将 last_open_time 置 0），然后不自动打开；可选弹出提示。
  - 若用户通过设置关闭此功能：始终不自动打开文件。

- 配合现有逻辑：
  - UI 配置仍由 QSettings 管理；自动打开开关本身也建议作为一个 QSettings 项（例如 `startup/openLastFile`）。
  - 文件具体是哪一个、滚动到什么位置，全部来自 SQLite。

### 10.2 具体算法（伪代码）

1. 程序启动时，完成 QSettings / UI 初始化后，初始化 `StateDbManager`。
2. 从 QSettings 读取开关：

   ```cpp
   QSettings s;
   const bool openLast = s.value("startup/openLastFile", true).toBool();
   if (!openLast) {
       return; // 用户关闭自动打开功能
   }
   ```

3. 从数据库查询“最近一次打开的文件”：

   ```sql
   SELECT path, scroll_ratio, file_mtime, last_file_size
   FROM documents
   WHERE last_open_time > 0
   ORDER BY last_open_time DESC
   LIMIT 1;
   ```

4. 若没有结果：直接返回。
5. 若有结果：
   - 检查该路径对应文件是否存在：

     ```cpp
     QFileInfo fi(pathFromDb);
     if (!fi.exists()) {
         stateDb.markDocumentMissing(pathFromDb); // 可选：将 last_open_time 置 0 或删除行
         return;
     }
     ```

   - 可选：对比 `file_mtime` / `last_file_size`，发现文件发生变化时，可决定：
     - 仍然按照旧的 `scroll_ratio` 恢复；
     - 或者重置为 0（从头开始），并将这一行为记入日志。

6. 调用现有“打开文件”逻辑：

   ```cpp
   openFile(pathFromDb);
   applyScrollRatio(scrollRatioFromDb);
   ```

   其中 `openFile()` 应该复用你当前 MainWindow / MarkdownPage 已有的打开流程；`applyScrollRatio()` 则是 WebView 上执行 JS 滚动的封装。

### 10.3 与历史记录的一致性

- 当程序自动打开最后一个文件时，仍然应当调用 `recordOpen(path)`，以更新：
  - `last_open_time`（最新启动时间）；
  - `file_mtime` / `last_file_size`。
- 这样可以保证：
  - 手动打开和自动打开路径走同一套记录逻辑；
  - 最近列表中始终以“最后一次成功打开”的时间排序。

---

## 11. 实现建议

- 新建一个 `StateDbManager` 类，负责：
  - 打开/初始化数据库；
  - 封装上文提到的所有高层操作；
  - 把路径规范化、事务、错误处理全部收敛在内部。

- MainWindow / MarkdownPage 等业务代码只调用：
  - `stateDb.recordOpen(filePath);`
  - `stateDb.loadScroll(filePath);`
  - `stateDb.updateScroll(filePath, ratio);`
  - `stateDb.listRecent(maxCount);`

这样可以避免数据库细节散落在各处，为以后 schema 迁移与重构保留空间。
