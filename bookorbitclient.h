#pragma once

#include <QJsonDocument>
#include <QHash>
#include <QList>
#include <QNetworkAccessManager>
#include <QObject>
#include <QSettings>
#include <QStringList>
#include <QVariantList>
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
    Q_PROPERTY(QVariantList books READ books NOTIFY booksChanged)
    Q_PROPERTY(int totalBooks READ totalBooks NOTIFY booksChanged)
    Q_PROPERTY(bool hasMoreBooks READ hasMoreBooks NOTIFY booksChanged)

public:
    using DataHandler = std::function<void(const QByteArray &)>;
    using ErrorHandler = std::function<void(int status, const QString &message)>;

    explicit BookOrbitClient(const QString &settingsPath, QObject *parent = nullptr);

    QString serverUrl() const { return m_serverUrl; }
    QString username() const { return m_username; }
    bool signedIn() const { return m_signedIn; }
    bool busy() const { return m_pending > 0; }
    QString error() const { return m_error; }
    QVariantList books() const { return m_books; }
    int totalBooks() const { return m_totalBooks; }
    bool hasMoreBooks() const { return m_books.size() < m_totalBooks; }

    // Loads the book list if there is a saved session (signedIn is already true then)
    Q_INVOKABLE void start();
    Q_INVOKABLE void signIn(const QString &username, const QString &password);
    Q_INVOKABLE void signOut();

    // How the book list is sorted and filtered, and so how it is split into sections:
    //   sortField  addedAt, title, author or lastReadAt (BookOrbit sort fields)
    //   formats    only books with one of these file formats; empty for all books
    // Reloads the list.
    Q_INVOKABLE void setBookQuery(const QString &sortField, const QStringList &formats);
    // Reloads the first page of the book list
    Q_INVOKABLE void refreshBooks();
    // Appends the next page, if there is one and none is loading
    Q_INVOKABLE void loadMoreBooks();

    // Authenticated GET for raw data such as cover thumbnails. Doesn't count as busy, so
    // background downloads don't redraw the status line.
    void download(const QString &path, DataHandler onSuccess, ErrorHandler onError);

signals:
    void usernameChanged();
    void signedInChanged();
    void busyChanged();
    void errorChanged();
    void booksChanged();

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
    // call() for JSON replies; errors go to the error property
    void callJson(const QByteArray &method, const QString &path, const QJsonDocument &body,
                  std::function<void(const QJsonDocument &)> onSuccess);
    void loadBooksPage(int page);
    // Loads every author's sort name (GET /authors), then calls then
    void loadAuthorSortNames(int page, std::function<void()> then);
    // Adds the fields the book rows show: section, sectionLabel,
    // statusText, seriesText, onDevice
    QVariantMap decorateBook(const QVariantMap &book) const;
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
    QVariantList m_books;
    int m_totalBooks = 0;
    QString m_sortField = "addedAt";
    QStringList m_formats;
    int m_loadedPage = -1;   // pages count from 0
    bool m_loadingPage = false;
    // Bumped by every reload, so pages from an older query are dropped
    int m_queryGeneration = 0;
    // Author name -> the name the server sorts by: its sortName, or the name if it has none
    QHash<QString, QString> m_authorSortNames;
    bool m_refreshing = false;
    QList<std::function<void()>> m_afterRefresh;
};
