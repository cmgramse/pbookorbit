#pragma once

#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <functional>

struct sqlite3;
struct sqlite3_stmt;

// The app's own data, in an SQLite database (pbookorbit.db, in the system config folder):
//
//   settings     key/value: the login (username, refreshToken) and the app's settings.
//                Values keep SQLite's type: text, 0/1, or a JSON array for a list.
//   books        a local cache of BookOrbit books, by BookOrbit id. Each row holds the
//                book's list entry as the server sent it, as JSON (data), so what's kept
//                about a book can change without changing the table. Fields worth
//                querying are generated columns pulled out of that JSON (title, series);
//                another is one ALTER TABLE ... ADD COLUMN ... GENERATED away.
//   book_files   the files of those books on the device, which the app downloaded or
//                found. A book can have two: an ebook and an audiobook.
//
// The SDK's libsqlite3 is the system's own (libcommon_utilities links it), so it is on
// every device. Qt's Sql module isn't used: the firmware needn't have it.
//
// If the database can't be opened, reads come back empty and writes do nothing, so the
// app still runs, signed out.
class Database
{
public:
    explicit Database(const QString &path);
    ~Database();
    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;

    // Settings; a missing key reads as "", false or an empty list
    QString text(const QString &key) const;
    void setText(const QString &key, const QString &value);
    bool flag(const QString &key) const;
    void setFlag(const QString &key, bool value);
    QStringList list(const QString &key) const;
    void setList(const QString &key, const QStringList &value);
    void remove(const QString &key);

    // Saves a book's list entry, replacing what was kept for it
    void saveBook(int bookId, const QVariantMap &entry);
    // A file of the book on the device. The book must be saved.
    void addBookFile(int bookId, const QString &path);
    void removeBookFile(const QString &path);
    // The book's files, whether or not they are still there
    QStringList bookFiles(int bookId) const;
    // Every book file recorded, whether or not it is still there
    QSet<QString> allBookFiles() const;
    // The books with files recorded, by title
    struct Book {
        int id;
        QVariantMap entry;
        QStringList files;
    };
    QList<Book> booksWithFiles() const;

    // Runs the writes in one transaction: one write to storage, and all or none of them
    void transaction(const std::function<void()> &writes);

private:
    // A prepared statement with its values bound, or nullptr if the SQL failed
    sqlite3_stmt *prepare(const char *sql, const QVariantList &values = {}) const;
    // Runs a statement that returns no rows
    void exec(const char *sql, const QVariantList &values = {});
    void logError(const char *what) const;

    sqlite3 *m_db = nullptr;
};
