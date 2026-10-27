#include "core/database.h"

#include "core/diagnostics.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <sqlite3.h>

static const char *SCHEMA = R"sql(
    CREATE TABLE IF NOT EXISTS settings (
        key   TEXT PRIMARY KEY,
        value
    ) WITHOUT ROWID;

    CREATE TABLE IF NOT EXISTS books (
        id      INTEGER PRIMARY KEY,                -- BookOrbit's book id
        data    TEXT NOT NULL CHECK (json_valid(data)),
        updated INTEGER NOT NULL,                   -- when data was saved, Unix time
        title   TEXT GENERATED ALWAYS AS (json_extract(data, '$.title')) VIRTUAL,
        series  TEXT GENERATED ALWAYS AS (json_extract(data, '$.seriesName')) VIRTUAL
    );

    CREATE TABLE IF NOT EXISTS book_files (
        path    TEXT PRIMARY KEY,
        book_id INTEGER NOT NULL REFERENCES books(id) ON DELETE CASCADE
    );
    CREATE INDEX IF NOT EXISTS book_files_book_id ON book_files(book_id);
)sql";

Database::Database(const QString &path)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    if (sqlite3_open_v2(path.toUtf8().constData(), &m_db,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK) {
        logError("open");
        sqlite3_close(m_db);
        m_db = nullptr;
        return;
    }
    if (sqlite3_exec(m_db, "PRAGMA foreign_keys = ON", nullptr, nullptr, nullptr) != SQLITE_OK
        || sqlite3_exec(m_db, SCHEMA, nullptr, nullptr, nullptr) != SQLITE_OK) {
        logError("create tables");
        sqlite3_close(m_db);
        m_db = nullptr;
    }
}

Database::~Database()
{
    sqlite3_close(m_db);
}

void Database::logError(const char *what) const
{
    diagnostics::log(QString("database: %1 failed: %2")
                         .arg(QString::fromUtf8(what), QString::fromUtf8(sqlite3_errmsg(m_db))));
}

sqlite3_stmt *Database::prepare(const char *sql, const QVariantList &values) const
{
    if (!m_db)
        return nullptr;
    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &statement, nullptr) != SQLITE_OK) {
        logError(sql);
        return nullptr;
    }
    for (int i = 0; i < values.size(); i++) {
        const QVariant &value = values.at(i);
        switch (value.typeId()) {
        case QMetaType::Bool:
        case QMetaType::Int:
        case QMetaType::LongLong:
            sqlite3_bind_int64(statement, i + 1, value.toLongLong());
            break;
        default:
            if (value.isNull()) {
                sqlite3_bind_null(statement, i + 1);
            } else {
                const QByteArray text = value.toString().toUtf8();
                sqlite3_bind_text(statement, i + 1, text.constData(), int(text.size()), SQLITE_TRANSIENT);
            }
        }
    }
    return statement;
}

void Database::exec(const char *sql, const QVariantList &values)
{
    sqlite3_stmt *statement = prepare(sql, values);
    if (!statement)
        return;
    if (sqlite3_step(statement) != SQLITE_DONE)
        logError(sql);
    sqlite3_finalize(statement);
}

// The text in a column of the current row
static QString columnText(sqlite3_stmt *statement, int column)
{
    return QString::fromUtf8(reinterpret_cast<const char *>(sqlite3_column_text(statement, column)),
                             sqlite3_column_bytes(statement, column));
}

QString Database::text(const QString &key) const
{
    QString value;
    if (sqlite3_stmt *statement = prepare("SELECT value FROM settings WHERE key = ?", {key})) {
        if (sqlite3_step(statement) == SQLITE_ROW)
            value = columnText(statement, 0);
        sqlite3_finalize(statement);
    }
    return value;
}

void Database::setText(const QString &key, const QString &value)
{
    exec("INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)", {key, value});
}

bool Database::flag(const QString &key) const
{
    bool value = false;
    if (sqlite3_stmt *statement = prepare("SELECT value FROM settings WHERE key = ?", {key})) {
        if (sqlite3_step(statement) == SQLITE_ROW)
            value = sqlite3_column_int(statement, 0) != 0;
        sqlite3_finalize(statement);
    }
    return value;
}

void Database::setFlag(const QString &key, bool value)
{
    exec("INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)", {key, value});
}

QStringList Database::list(const QString &key) const
{
    QStringList value;
    for (const QJsonValue &item : QJsonDocument::fromJson(text(key).toUtf8()).array())
        value.append(item.toString());
    return value;
}

void Database::setList(const QString &key, const QStringList &value)
{
    setText(key, QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(value)).toJson(QJsonDocument::Compact)));
}

void Database::remove(const QString &key)
{
    exec("DELETE FROM settings WHERE key = ?", {key});
}

void Database::saveBook(int bookId, const QVariantMap &entry)
{
    const QString data = QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(entry)).toJson(QJsonDocument::Compact));
    exec("INSERT INTO books (id, data, updated) VALUES (?, ?, unixepoch()) "
         "ON CONFLICT (id) DO UPDATE SET data = excluded.data, updated = excluded.updated",
         {bookId, data});
}

void Database::addBookFile(int bookId, const QString &path)
{
    exec("INSERT OR REPLACE INTO book_files (path, book_id) VALUES (?, ?)", {path, bookId});
}

void Database::removeBookFile(const QString &path)
{
    exec("DELETE FROM book_files WHERE path = ?", {path});
}

QStringList Database::bookFiles(int bookId) const
{
    QStringList paths;
    // In the order they were added
    if (sqlite3_stmt *statement = prepare("SELECT path FROM book_files WHERE book_id = ? ORDER BY rowid", {bookId})) {
        while (sqlite3_step(statement) == SQLITE_ROW)
            paths.append(columnText(statement, 0));
        sqlite3_finalize(statement);
    }
    return paths;
}

QSet<QString> Database::allBookFiles() const
{
    QSet<QString> paths;
    if (sqlite3_stmt *statement = prepare("SELECT path FROM book_files")) {
        while (sqlite3_step(statement) == SQLITE_ROW)
            paths.insert(columnText(statement, 0));
        sqlite3_finalize(statement);
    }
    return paths;
}

QList<Database::Book> Database::booksWithFiles() const
{
    QList<Book> books;
    if (sqlite3_stmt *statement = prepare(
            "SELECT books.id, books.data, book_files.path FROM books "
            "JOIN book_files ON book_files.book_id = books.id "
            "ORDER BY books.title COLLATE NOCASE, books.id, book_files.rowid")) {
        while (sqlite3_step(statement) == SQLITE_ROW) {
            const int id = sqlite3_column_int(statement, 0);
            if (books.isEmpty() || books.last().id != id) {
                const QJsonObject entry = QJsonDocument::fromJson(columnText(statement, 1).toUtf8()).object();
                books.append(Book{id, entry.toVariantMap(), {}});
            }
            books.last().files.append(columnText(statement, 2));
        }
        sqlite3_finalize(statement);
    }
    return books;
}

void Database::transaction(const std::function<void()> &writes)
{
    exec("BEGIN");
    writes();
    exec("COMMIT");
}
