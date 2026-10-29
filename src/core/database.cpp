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

    CREATE TABLE IF NOT EXISTS book_details (
        id      INTEGER PRIMARY KEY REFERENCES books(id) ON DELETE CASCADE,
        data    TEXT NOT NULL CHECK (json_valid(data)),
        updated INTEGER NOT NULL
    );

    CREATE TABLE IF NOT EXISTS authors (
        id      INTEGER PRIMARY KEY,                -- BookOrbit's author id
        name    TEXT NOT NULL,
        -- From the authors list and GET /authors/:id, either NULL until fetched
        entry   TEXT CHECK (entry IS NULL OR json_valid(entry)),
        details TEXT CHECK (details IS NULL OR json_valid(details)),
        updated INTEGER NOT NULL
    );
    CREATE INDEX IF NOT EXISTS authors_name ON authors(name);

    CREATE TABLE IF NOT EXISTS collections (
        id       INTEGER PRIMARY KEY,               -- BookOrbit's collection id
        position INTEGER NOT NULL,                  -- in the user's order
        data     TEXT NOT NULL CHECK (json_valid(data))
    );

    CREATE TABLE IF NOT EXISTS collection_books (
        collection_id INTEGER NOT NULL REFERENCES collections(id) ON DELETE CASCADE,
        position      INTEGER NOT NULL,             -- newest first
        book_id       INTEGER NOT NULL,
        PRIMARY KEY (collection_id, position)
    ) WITHOUT ROWID;

    CREATE TABLE IF NOT EXISTS list_cache (
        name TEXT PRIMARY KEY,                      -- "library", "collections", ...
        data TEXT NOT NULL CHECK (json_valid(data))
    ) WITHOUT ROWID;
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

// JSON text for a map
static QString json(const QVariantMap &map)
{
    return QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(map)).toJson(QJsonDocument::Compact));
}

// Text of a column in the current row
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
    const QString data = json(entry);
    exec("INSERT INTO books (id, data, updated) VALUES (?, ?, unixepoch()) "
         "ON CONFLICT (id) DO UPDATE SET data = excluded.data, updated = excluded.updated",
         {bookId, data});
}

int Database::updateBooks(const QList<QVariantMap> &entries)
{
    int changed = 0;
    for (const QVariantMap &entry : entries) {
        exec("UPDATE books SET data = ?2, updated = unixepoch() WHERE id = ?1 AND data IS NOT ?2",
             {entry.value("id").toInt(), json(entry)});
        if (m_db)
            changed += sqlite3_changes(m_db);
    }
    return changed;
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
    // In insertion order
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

// A map from JSON text, empty for NULL
static QVariantMap jsonMap(const QString &text)
{
    return QJsonDocument::fromJson(text.toUtf8()).object().toVariantMap();
}

void Database::saveBookDetails(int bookId, const QVariantMap &details)
{
    exec("INSERT INTO book_details (id, data, updated) "
         "SELECT ?1, ?2, unixepoch() WHERE EXISTS (SELECT 1 FROM books WHERE id = ?1) "
         "ON CONFLICT (id) DO UPDATE SET data = excluded.data, updated = excluded.updated",
         {bookId, json(details)});
}

QVariantMap Database::bookDetails(int bookId) const
{
    QVariantMap details;
    if (sqlite3_stmt *statement = prepare("SELECT data FROM book_details WHERE id = ?", {bookId})) {
        if (sqlite3_step(statement) == SQLITE_ROW)
            details = jsonMap(columnText(statement, 0));
        sqlite3_finalize(statement);
    }
    return details;
}

void Database::saveListCache(const QString &name, const QVariantMap &data)
{
    exec("INSERT OR REPLACE INTO list_cache (name, data) VALUES (?, ?)", {name, json(data)});
}

QVariantMap Database::listCache(const QString &name) const
{
    QVariantMap data;
    if (sqlite3_stmt *statement = prepare("SELECT data FROM list_cache WHERE name = ?", {name})) {
        if (sqlite3_step(statement) == SQLITE_ROW)
            data = jsonMap(columnText(statement, 0));
        sqlite3_finalize(statement);
    }
    return data;
}

void Database::clearListCaches()
{
    exec("DELETE FROM list_cache");
}

void Database::saveAuthors(const QList<QVariantMap> &entries)
{
    for (const QVariantMap &entry : entries) {
        exec("INSERT INTO authors (id, name, entry, updated) VALUES (?, ?, ?, unixepoch()) "
             "ON CONFLICT (id) DO UPDATE SET name = excluded.name, entry = excluded.entry, "
             "updated = excluded.updated",
             {entry.value("id").toInt(), entry.value("name").toString(), json(entry)});
    }
}

void Database::saveAuthorDetails(int authorId, const QVariantMap &details)
{
    exec("INSERT INTO authors (id, name, details, updated) VALUES (?, ?, ?, unixepoch()) "
         "ON CONFLICT (id) DO UPDATE SET name = excluded.name, details = excluded.details, "
         "updated = excluded.updated",
         {authorId, details.value("name").toString(), json(details)});
}

QHash<QString, Database::Author> Database::authorsNamed(const QStringList &names) const
{
    QHash<QString, Author> authors;
    const QString list = QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(names)).toJson(QJsonDocument::Compact));
    if (sqlite3_stmt *statement = prepare(
            "SELECT id, name, entry, details FROM authors "
            "WHERE name IN (SELECT value FROM json_each(?))", {list})) {
        while (sqlite3_step(statement) == SQLITE_ROW) {
            authors.insert(columnText(statement, 1),
                           Author{sqlite3_column_int(statement, 0), jsonMap(columnText(statement, 2)),
                                  jsonMap(columnText(statement, 3))});
        }
        sqlite3_finalize(statement);
    }
    return authors;
}

QVariantMap Database::authorDetails(int authorId) const
{
    QVariantMap details;
    if (sqlite3_stmt *statement = prepare("SELECT details FROM authors WHERE id = ?", {authorId})) {
        if (sqlite3_step(statement) == SQLITE_ROW)
            details = jsonMap(columnText(statement, 0));
        sqlite3_finalize(statement);
    }
    return details;
}

void Database::saveCollections(const QList<QVariantMap> &entries)
{
    QJsonArray ids;
    for (const QVariantMap &entry : entries)
        ids.append(entry.value("id").toInt());
    exec("DELETE FROM collections WHERE id NOT IN (SELECT value FROM json_each(?))",
         {QString::fromUtf8(QJsonDocument(ids).toJson(QJsonDocument::Compact))});
    for (int i = 0; i < entries.size(); i++) {
        exec("INSERT INTO collections (id, position, data) VALUES (?, ?, ?) "
             "ON CONFLICT (id) DO UPDATE SET position = excluded.position, data = excluded.data",
             {entries.at(i).value("id").toInt(), i, json(entries.at(i))});
    }
}

void Database::saveCollectionBooks(int collectionId, const QVariantList &bookIds)
{
    exec("DELETE FROM collection_books WHERE collection_id = ?", {collectionId});
    for (int i = 0; i < bookIds.size(); i++) {
        exec("INSERT INTO collection_books (collection_id, position, book_id) VALUES (?, ?, ?)",
             {collectionId, i, bookIds.at(i).toInt()});
    }
}

QList<Database::Collection> Database::collections() const
{
    QList<Collection> collections;
    if (sqlite3_stmt *statement = prepare(
            "SELECT collections.id, collections.data, collection_books.book_id FROM collections "
            "LEFT JOIN collection_books ON collection_books.collection_id = collections.id "
            "ORDER BY collections.position, collection_books.position")) {
        int lastId = -1;
        while (sqlite3_step(statement) == SQLITE_ROW) {
            const int id = sqlite3_column_int(statement, 0);
            if (collections.isEmpty() || lastId != id)
                collections.append(Collection{jsonMap(columnText(statement, 1)), {}});
            lastId = id;
            if (sqlite3_column_type(statement, 2) != SQLITE_NULL)
                collections.last().bookIds.append(sqlite3_column_int(statement, 2));
        }
        sqlite3_finalize(statement);
    }
    return collections;
}

void Database::transaction(const std::function<void()> &writes)
{
    exec("BEGIN");
    writes();
    exec("COMMIT");
}
