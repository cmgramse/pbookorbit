#pragma once

#include "booklistmodel.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QHash>
#include <QSet>
#include <QList>
#include <QNetworkAccessManager>
#include <QObject>
#include <QSettings>
#include <QStringList>
#include <functional>

class QNetworkReply;

// Talks to a BookOrbit server through its web API, signed in as a normal BookOrbit user.
//
// Sign-in uses a "native" session: POST /auth/login returns a short-lived access token
// (sent as "Authorization: Bearer") and a refresh token in the response body. Only the
// refresh token is saved; each refresh hands out a new one.
class BookOrbitClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString serverUrl READ serverUrl CONSTANT)
    Q_PROPERTY(QString username READ username NOTIFY usernameChanged)
    Q_PROPERTY(bool signedIn READ signedIn NOTIFY signedInChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(BookListModel *books READ books CONSTANT)
    Q_PROPERTY(int totalBooks READ totalBooks NOTIFY booksChanged)
    // Sections to jump to, for the A-Z sorts: [{label: "A", index: 2}, ...] in list order.
    // Empty for the date sorts.
    Q_PROPERTY(QVariantList jumpSections READ jumpSections NOTIFY jumpSectionsChanged)

public:
    using DataHandler = std::function<void(const QByteArray &)>;
    using ErrorHandler = std::function<void(int status, const QString &message)>;

    explicit BookOrbitClient(const QString &settingsPath, QObject *parent = nullptr);

    QString serverUrl() const { return m_serverUrl; }
    QString username() const { return m_username; }
    bool signedIn() const { return m_signedIn; }
    bool busy() const { return m_pending > 0; }
    QString error() const { return m_error; }
    BookListModel *books() { return &m_books; }
    int totalBooks() const { return m_totalBooks; }
    QVariantList jumpSections() const { return m_jumpSections; }

    // Loads the book list if there is a saved session (signedIn is already true then)
    Q_INVOKABLE void start();
    Q_INVOKABLE void signIn(const QString &username, const QString &password);
    Q_INVOKABLE void signOut();

    // How the book list is sorted and filtered, and so how it is split into sections:
    //   sortField  addedAt, title, author or lastReadAt (BookOrbit sort fields)
    //   formats    only books with one of these file formats; empty for all books
    // Reloads the list.
    Q_INVOKABLE void setBookQuery(const QString &sortField, const QStringList &formats);
    // Reloads the book list from the first page
    Q_INVOKABLE void refreshBooks();
    // Loads the pages holding rows first to last, if they aren't loaded or loading. The
    // list calls it for the rows on screen and a screenful either side as it scrolls.
    Q_INVOKABLE void loadBooks(int first, int last);

    // Authenticated GET for raw data such as cover thumbnails. Doesn't count as busy, so
    // background downloads don't redraw the status line.
    void download(const QString &path, DataHandler onSuccess, ErrorHandler onError);

signals:
    void usernameChanged();
    void signedInChanged();
    void busyChanged();
    void errorChanged();
    void booksChanged();
    void jumpSectionsChanged();

private:
    // Sends a request; withToken adds the access token (login, refresh and logout go
    // without); quiet requests don't count as busy
    void send(const QByteArray &method, const QString &path, const QJsonDocument &body,
              DataHandler onSuccess, ErrorHandler onError, bool withToken = false,
              bool quiet = false);
    // Sends a request with the access token, refreshing it once if the server rejects it
    void call(const QByteArray &method, const QString &path, const QJsonDocument &body,
              DataHandler onSuccess, ErrorHandler onError, bool quiet = false,
              bool retried = false);
    // call() for JSON replies; errors go to the error property, then to onFailed if given
    void callJson(const QByteArray &method, const QString &path, const QJsonDocument &body,
                  std::function<void(const QJsonDocument &)> onSuccess,
                  std::function<void()> onFailed = nullptr);
    // The library the list shows (GET /libraries), then calls then
    void loadLibrary(std::function<void()> then);
    // The sort, and the filter if there is one, as BookOrbit's book queries take them
    QJsonObject bookQuery() const;
    // Loads the A-Z sections (jump buckets), then calls then
    void loadJumpSections(std::function<void()> then);
    void loadBooksPage(int page);
    // Adds the fields the book rows show: section, sectionLabel, statusText, seriesText,
    // onDevice
    QVariantMap decorateBook(const QVariantMap &book) const;
    // Works out which of rows first to last start a section, so they show its box
    void updateSectionStarts(int first, int last);
    bool letterSections() const { return m_sortField == "title" || m_sortField == "author"; }
    // Swaps the refresh token for a new access token, then runs the callbacks queued meanwhile
    void refreshSession(std::function<void()> then);
    void applyCredentials(const QJsonObject &credentials);
    void clearSession();
    void setSignedIn(bool signedIn);
    void setError(const QString &error);

    QSettings m_settings;
    QNetworkAccessManager m_nam;
    QString m_serverUrl;
    QString m_username;
    QString m_accessToken;
    QString m_refreshToken;
    bool m_signedIn = false;
    int m_pending = 0;
    QString m_error;
    BookListModel m_books;
    int m_totalBooks = 0;
    QString m_sortField = "addedAt";
    QStringList m_formats;
    int m_libraryId = -1;
    // Pages (counting from 0) loaded, and asked for, for the current query
    QSet<int> m_loadedPages;
    QSet<int> m_loadingPages;
    // Bumped by every reload, so replies to an older query are dropped
    int m_queryGeneration = 0;
    // The query the list's rows were made for: until the first page of a new query
    // arrives, the list still shows the old one
    int m_listGeneration = -1;
    QVariantList m_jumpSections;
    // For the A-Z sorts: the first row of each section -> its letter
    QHash<int, QString> m_sectionStarts;
    bool m_refreshing = false;
    QList<std::function<void()>> m_afterRefresh;
};
