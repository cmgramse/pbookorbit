#pragma once

#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <functional>

struct sqlite3;
struct sqlite3_stmt;

// The app's SQLite database (pbookorbit.db, in the system config folder):
//
//   settings     key/value: the login (username, refreshToken) and app settings. Values
//                are text, 0/1, or a JSON array for lists.
//   books        cache of BookOrbit books by id. data holds the server's list entry as
//                JSON, so the stored fields can change without a schema change. Fields
//                worth querying (title, series) are generated columns; add more with
//                ALTER TABLE ... ADD COLUMN ... GENERATED.
//   book_files   files of those books on the device, downloaded or found. A book can have
//                two: an ebook and an audiobook.
//
// Uses the system libsqlite3 (linked by libcommon_utilities), which every device has. Qt's
// Sql module isn't used because the firmware may not ship it.
//
// If the database can't be opened, reads return empty and writes do nothing, so the app
// still runs, signed out.
class Database
{
public:
    explicit Database(const QString &path);
    ~Database();
    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;

    // Settings. A missing key reads as "", false or an empty list.
    QString text(const QString &key) const;
    void setText(const QString &key, const QString &value);
    bool flag(const QString &key) const;
    void setFlag(const QString &key, bool value);
    QStringList list(const QString &key) const;
    void setList(const QString &key, const QStringList &value);
    void remove(const QString &key);

    // Saves a book's list entry, replacing any existing one
    void saveBook(int bookId, const QVariantMap &entry);
    // Records a downloaded file for a book. The book must already be saved.
    void addBookFile(int bookId, const QString &path);
    void removeBookFile(const QString &path);
    // The book's recorded files, whether or not they still exist
    QStringList bookFiles(int bookId) const;
    // Every recorded book file, whether or not it still exists
    QSet<QString> allBookFiles() const;
    // Books with recorded files, by title
    struct Book {
        int id;
        QVariantMap entry;
        QStringList files;
    };
    QList<Book> booksWithFiles() const;

    // Runs writes in a single transaction: one write to storage, all or nothing
    void transaction(const std::function<void()> &writes);

private:
    // A prepared statement with its values bound, or nullptr if the SQL failed
    sqlite3_stmt *prepare(const char *sql, const QVariantList &values = {}) const;
    // Runs a statement that returns no rows
    void exec(const char *sql, const QVariantList &values = {});
    void logError(const char *what) const;

    sqlite3 *m_db = nullptr;
};
