#pragma once

#include "booklistmodel.h"

#include <QColor>
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
    // Books matching the search text, by title
    Q_PROPERTY(BookListModel *searchResults READ searchResults CONSTANT)
    // How many books match the search text, or -1 until the server has said
    Q_PROPERTY(int searchTotal READ searchTotal NOTIFY searchChanged)
    // The books showBooksOf() asked for: all of an author's, or a series'
    Q_PROPERTY(BookListModel *booksOf READ booksOf CONSTANT)
    // How many there are, or -1 until the server has said
    Q_PROPERTY(int booksOfTotal READ booksOfTotal NOTIFY booksOfChanged)
    // jumpSections for booksOf: for an author's books (by title) the letters, for a
    // series' the numbers in the series: [{label: "12", index: 11}, ...]
    Q_PROPERTY(QVariantList booksOfJumpSections READ booksOfJumpSections NOTIFY jumpSectionsChanged)
    // The user's BookOrbit accent colour (its "Accent color" appearance setting), and the
    // colour for text and icons on it. Only known if they sync (or once synced) their theme
    // preferences to the server; otherwise BookOrbit's default, teal. Kept for offline use.
    Q_PROPERTY(QColor accentColor READ accentColor NOTIFY accentChanged)
    Q_PROPERTY(QColor accentTextColor READ accentTextColor NOTIFY accentChanged)
    // Searches kept for the search page, newest first
    Q_PROPERTY(QStringList recentSearches READ recentSearches NOTIFY recentSearchesChanged)

public:
    using DataHandler = std::function<void(const QByteArray &)>;
    using ErrorHandler = std::function<void(int status, const QString &message)>;

    explicit BookOrbitClient(const QString &settingsPath, QObject *parent = nullptr);

    QString serverUrl() const { return m_serverUrl; }
    QString username() const { return m_username; }
    bool signedIn() const { return m_signedIn; }
    bool busy() const { return m_pending > 0; }
    QString error() const { return m_error; }
    BookListModel *books() { return &m_list.model; }
    int totalBooks() const { return m_list.total; }
    QVariantList jumpSections() const { return m_list.jumpSections; }
    BookListModel *searchResults() { return &m_search.model; }
    int searchTotal() const;
    BookListModel *booksOf() { return &m_booksOf.model; }
    int booksOfTotal() const;
    QVariantList booksOfJumpSections() const { return m_booksOf.jumpSections; }
    QStringList recentSearches() const { return m_recentSearches; }
    QColor accentColor() const;
    QColor accentTextColor() const;

    // Loads the book list if there is a saved session (signedIn is already true then)
    Q_INVOKABLE void start();
    Q_INVOKABLE void signIn(const QString &username, const QString &password);
    Q_INVOKABLE void signOut();

    // How the book list is sorted and filtered, and so how it is split into sections:
    //   sortField   addedAt, title, author or lastReadAt (BookOrbit sort fields)
    //   descending  Z to A, or newest first
    //   formats     only books with one of these file formats; empty for all books
    //   onlyListed  only the books in onlyBooks, listed from there without the server
    //               (so offline too): the downloaded books, as Downloads.downloadedBooks()
    //               gives them. The list sorts and filters them itself.
    // Reloads the list.
    Q_INVOKABLE void setBookQuery(const QString &sortField, bool descending,
                                  const QStringList &formats, bool onlyListed = false,
                                  const QVariantList &onlyBooks = {});
    // Reloads the book list from the first page
    Q_INVOKABLE void refreshBooks();
    // Loads the pages holding rows first to last, if they aren't loaded or loading. The
    // list calls it for the rows on screen and a screenful either side as it scrolls.
    Q_INVOKABLE void loadBooks(int first, int last);

    // Searches the library's titles, authors and series. Shorter than MIN_SEARCH_LENGTH
    // characters clears the results instead.
    Q_INVOKABLE void search(const QString &text);
    // loadBooks() for the search results
    Q_INVOKABLE void loadSearchResults(int first, int last);
    // Lists every book by any of the authors (field "author", values their names), or
    // in the series (field "series", values its name) in booksOf: by title, or for a series
    // in its order
    Q_INVOKABLE void showBooksOf(const QString &field, const QStringList &values);
    // loadBooks() for booksOf
    Q_INVOKABLE void loadBooksOf(int first, int last);
    // Fetches a book's full details (GET /books/:id: description, publisher, genres...);
    // bookDetails(bookId, details) follows, with empty details if it failed
    Q_INVOKABLE void loadBookDetails(int bookId);

    // Puts text at the top of the recent searches, or moves it there if it is already in
    // them (ignoring case), and saves them
    Q_INVOKABLE void addRecentSearch(const QString &text);

    // Authenticated GET for raw data such as cover thumbnails. Doesn't count as busy, so
    // background downloads don't redraw the status line.
    void download(const QString &path, DataHandler onSuccess, ErrorHandler onError);
    // Authenticated GET for a large file, such as a book, that the caller reads as it
    // arrives: gets a new access token first (so it lasts the whole download), then starts
    // the request and hands over the reply, which the caller then owns
    void startStream(const QString &path, std::function<void(QNetworkReply *)> onStarted,
                     ErrorHandler onError);
    // Authenticated request for background work: failures go to onError rather than the
    // error property, and it doesn't count as busy
    void request(const QByteArray &method, const QString &path, const QJsonDocument &body,
                 DataHandler onSuccess, ErrorHandler onError);
    // The library the book list shows, looked up first if it isn't known yet
    void withLibrary(std::function<void(int libraryId)> then, ErrorHandler onError);

signals:
    void usernameChanged();
    void signedInChanged();
    void busyChanged();
    void errorChanged();
    void booksChanged();
    void jumpSectionsChanged();
    void searchChanged();
    void booksOfChanged();
    void bookDetails(int bookId, const QVariantMap &details);
    void recentSearchesChanged();
    void accentChanged();

private:
    // How a list is split into sections: not at all, A-Z (from the server's jump buckets),
    // by day (for the date sorts), or a section for each book of a series headed by its
    // number in the series (whose rows then leave the series out)
    enum class Sections { None, Letters, Days, SeriesNumbers };

    // A list of books loaded a page at a time: the library, or search results. The model
    // has a row for every book from the first page on; other pages load as they are needed.
    struct PagedBooks {
        BookListModel model;
        // Sort, filter and search text, as BookOrbit's book queries take them
        QJsonObject query;
        // Pages (counting from 0) loaded, and asked for, for the current query
        QSet<int> loadedPages;
        QSet<int> loadingPages;
        // Bumped by every reload, so replies to an older query are dropped
        int queryGeneration = 0;
        // The query the rows were made for: until the first page of a new query arrives,
        // the list still shows the old one
        int listGeneration = -1;
        int total = 0;
        // Hide the audiobook record of a book whose ebook is a separate record, so the
        // ebook's row stands for both. Only where the ebook is sure to be in the list too.
        bool mergePairs = false;
        Sections sections = Sections::None;
        // The sections to jump to (see jumpSections), and for Letters the first row of
        // each -> its letter
        QVariantList jumpSections;
        QHash<int, QString> sectionStarts;
    };

    // On the device, asks the system to bring up Wi-Fi if it isn't connected
    void ensureNetwork();
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
    // Reads the user's accent from their synced theme preferences, if any
    void loadAccent();
    // The library the list shows (GET /libraries), then calls then
    void loadLibrary(std::function<void()> then);
    // The sort, and the filter if there is one, as BookOrbit's book queries take them
    QJsonObject bookQuery() const;
    // Fills the list with onlyBooks: filtered by the downloaded file's format, sorted, and
    // split into sections like the server's lists
    void showListedBooks();
    // Loads the A-Z sections (jump buckets) for a list's query, then calls then
    void loadJumpSections(PagedBooks &list, std::function<void()> then);
    // Loads the series numbers of every book in a list sorted by them, as its jumpSections,
    // from page (of SERIES_NUMBERS_PAGE_SIZE) on
    void loadSeriesSections(PagedBooks &list, int page = 0);
    // Drops a list's sections to jump to
    void clearJumpSections(PagedBooks &list);
    // Starts a new query for a list: replies to the old one are dropped from now on
    void restartPages(PagedBooks &list);
    void loadPages(PagedBooks &list, int first, int last);
    void loadPage(PagedBooks &list, int page);
    // Finds the other records of the books on a page just loaded (an audiobook kept as a
    // separate record from its ebook, say: same title and first author) and adds their
    // formats to the rows
    void lookUpCompanions(PagedBooks *list, int first, const QVariantList &books, int generation);
    // Adds the fields the book rows show: section, sectionLabel, statusText, seriesText,
    // and variants, formats and hidden (see addCompanions)
    QVariantMap decorateBook(const QVariantMap &book) const;
    // Adds other records of the same book to a row: their versions to its variants and
    // their formats to its formats. Hides the row (an audiobook record whose ebook is
    // listed too) if hideAudio.
    static void addCompanions(QVariantMap &row, const QList<QVariantMap> &companions, bool hideAudio);
    // Works out which of a list's rows first to last start a section, so they show its box
    void updateSectionStarts(PagedBooks &list, int first, int last);
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
    int m_libraryId = -1;
    // The library's book list
    PagedBooks m_list;
    QString m_sortField = "title";   // matches the sort menu's default, Title A to Z
    bool m_sortDescending = false;
    bool m_onlyListed = false;
    QVariantList m_onlyBooks;
    QStringList m_formats;
    PagedBooks m_search;
    PagedBooks m_booksOf;
    QStringList m_recentSearches;
    QString m_accent;   // a BookOrbit accent name, e.g. "teal"; empty for the default
    bool m_refreshing = false;
    QList<std::function<void()>> m_afterRefresh;
};
