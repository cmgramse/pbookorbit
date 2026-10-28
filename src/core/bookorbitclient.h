#pragma once

#include "core/booklistmodel.h"

#include <QColor>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHash>
#include <QSet>
#include <QList>
#include <QNetworkAccessManager>
#include <QObject>
#include <QStringList>
#include <functional>

class Database;
class QNetworkReply;

// Client for the BookOrbit web API, signed in as an ordinary user.
//
// Uses a "native" session: POST /auth/login returns a short-lived access token (sent as a
// Bearer token) and a refresh token. Only the refresh token is saved, and every refresh
// replaces it.
class BookOrbitClient : public QObject
{
    Q_OBJECT
    // Last server signed in to, e.g. "https://books.example.com". Empty until a sign-in
    // succeeds, and kept after signing out.
    Q_PROPERTY(QString serverUrl READ serverUrl NOTIFY serverUrlChanged)
    // Prefill for the server field when none is saved. Set at build time with
    // SUGGESTED_SERVER, otherwise empty.
    Q_PROPERTY(QString suggestedServer READ suggestedServer CONSTANT)
    Q_PROPERTY(QString username READ username NOTIFY usernameChanged)
    Q_PROPERTY(bool signedIn READ signedIn NOTIFY signedInChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    // Whether the server can be reached, as far as the app knows. Starts true; goes false
    // when a request gets no reply or Wi-Fi can't reconnect without asking, and true again
    // on any reply. While false, requests other than goOnline(), sign-in and downloads
    // fail at once with NO_CONNECTION.
    Q_PROPERTY(bool online READ online NOTIFY onlineChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(BookListModel *books READ books CONSTANT)
    Q_PROPERTY(int totalBooks READ totalBooks NOTIFY booksChanged)
    // Jump rail sections for the A-Z sorts, e.g. [{label: "A", index: 2}, ...]. Empty for
    // date sorts.
    Q_PROPERTY(QVariantList jumpSections READ jumpSections NOTIFY jumpSectionsChanged)
    // Search results, by title
    Q_PROPERTY(BookListModel *searchResults READ searchResults CONSTANT)
    // Number of matches, or -1 until known
    Q_PROPERTY(int searchTotal READ searchTotal NOTIFY searchChanged)
    // Books listed by showBooksOf()
    Q_PROPERTY(BookListModel *booksOf READ booksOf CONSTANT)
    // Number of books, or -1 until known
    Q_PROPERTY(int booksOfTotal READ booksOfTotal NOTIFY booksOfChanged)
    // The user's collections, one row each (role "book"): {id, name, bookCount, covers
    // (the last two added books with covers), authors (by surname), formats, bookIds
    // (newest first)}. Filled in as each collection's books load; bookCount then counts a
    // separate ebook and audiobook record as one book.
    Q_PROPERTY(BookListModel *collections READ collections CONSTANT)
    // Letter sections when sorted by name, else empty. The first row of each section has
    // sectionStart and sectionLabel set.
    Q_PROPERTY(QVariantList collectionJumpSections READ collectionJumpSections NOTIFY collectionsChanged)
    // Whether collections have loaded since sign-in
    Q_PROPERTY(bool collectionsLoaded READ collectionsLoaded NOTIFY collectionsChanged)
    // The library's series, in rows like the collections': {id, name, bookCount, covers
    // (first two books), authors, formats (filled in later), lastAddedAt, readProgress
    // (0 to 1)}. Once formats are in, bookCount counts an ebook/audiobook pair once.
    Q_PROPERTY(BookListModel *seriesList READ seriesList CONSTANT)
    Q_PROPERTY(QVariantList seriesJumpSections READ seriesJumpSections NOTIFY seriesListChanged)
    Q_PROPERTY(bool seriesLoaded READ seriesLoaded NOTIFY seriesListChanged)
    // The library's authors: {kind: "author", id, name, sortName ("Martin, George R.R."),
    // bookCount, photo (imageUrl, or ""), lastAddedAt, formats, seriesCount (-1 until their
    // books are looked up)}. With a filter set, counts and formats cover only matching
    // books.
    Q_PROPERTY(BookListModel *authorsList READ authorsList CONSTANT)
    Q_PROPERTY(QVariantList authorJumpSections READ authorJumpSections NOTIFY authorsChanged)
    Q_PROPERTY(bool authorsLoaded READ authorsLoaded NOTIFY authorsChanged)
    // Whether every author's books have been looked up. Filtering needs this.
    Q_PROPERTY(bool authorBooksLoaded READ authorBooksLoaded NOTIFY authorsChanged)
    // One author's series (see showAuthorSeries()), in rows like seriesList's
    Q_PROPERTY(BookListModel *authorSeriesList READ authorSeriesList CONSTANT)
    Q_PROPERTY(QVariantList authorSeriesJumpSections READ authorSeriesJumpSections NOTIFY authorSeriesChanged)
    Q_PROPERTY(bool authorSeriesLoaded READ authorSeriesLoaded NOTIFY authorSeriesChanged)
    // Jump sections for booksOf: letters for an author's books, series numbers for a
    // series (e.g. [{label: "12", index: 11}, ...])
    Q_PROPERTY(QVariantList booksOfJumpSections READ booksOfJumpSections NOTIFY jumpSectionsChanged)
    // The user's BookOrbit accent colour and a readable text colour on top of it. Only
    // available if they've synced their theme to the server, otherwise teal (BookOrbit's
    // default). Cached for offline use.
    Q_PROPERTY(QColor accentColor READ accentColor NOTIFY accentChanged)
    Q_PROPERTY(QColor accentTextColor READ accentTextColor NOTIFY accentChanged)
    // Recent searches, newest first
    Q_PROPERTY(QStringList recentSearches READ recentSearches NOTIFY recentSearchesChanged)

public:
    using DataHandler = std::function<void(const QByteArray &)>;
    using ErrorHandler = std::function<void(int status, const QString &message)>;
    // The error for requests that can't be sent, with status 0
    static inline const QString NO_CONNECTION = QStringLiteral("No connection");

    explicit BookOrbitClient(Database *database, QObject *parent = nullptr);

    QString serverUrl() const { return m_serverUrl; }
    QString suggestedServer() const;
    QString username() const { return m_username; }
    bool signedIn() const { return m_signedIn; }
    bool busy() const { return m_pending > 0; }
    bool online() const { return m_online; }
    QString error() const { return m_error; }
    BookListModel *books() { return &m_list.model; }
    int totalBooks() const { return m_list.total; }
    QVariantList jumpSections() const { return m_list.jumpSections; }
    BookListModel *searchResults() { return &m_search.model; }
    int searchTotal() const;
    BookListModel *booksOf() { return &m_booksOf.model; }
    int booksOfTotal() const;
    QVariantList booksOfJumpSections() const { return m_booksOf.jumpSections; }
    BookListModel *collections() { return &m_collections.model; }
    bool collectionsLoaded() const { return m_collections.loaded; }
    QVariantList collectionJumpSections() const { return m_collections.jumpSections; }
    BookListModel *seriesList() { return &m_series.model; }
    bool seriesLoaded() const { return m_series.loaded; }
    QVariantList seriesJumpSections() const { return m_series.jumpSections; }
    BookListModel *authorsList() { return &m_authors.model; }
    bool authorsLoaded() const { return m_authors.loaded; }
    bool authorBooksLoaded() const { return m_authors.booksLoaded; }
    QVariantList authorJumpSections() const { return m_authors.jumpSections; }
    BookListModel *authorSeriesList() { return &m_authorSeries.model; }
    bool authorSeriesLoaded() const { return m_authorSeries.loaded; }
    QVariantList authorSeriesJumpSections() const { return m_authorSeries.jumpSections; }
    QStringList recentSearches() const { return m_recentSearches; }
    QColor accentColor() const;
    QColor accentTextColor() const;

    // Loads the book list if a session was saved
    Q_INVOKABLE void start();
    // server is as typed, e.g. "books.example.com". Defaults to https.
    Q_INVOKABLE void signIn(const QString &server, const QString &username, const QString &password);
    // Clears the saved server so another can be used. Only when signed out.
    Q_INVOKABLE void forgetServer();
    Q_INVOKABLE void signOut();
    // Asks the system to connect Wi-Fi (its own dialog), then checks the server is there
    Q_INVOKABLE void goOnline();

    // Sets the book list's sort and filter, and reloads it.
    //   sortField   addedAt, title, author or lastReadAt
    //   descending  Z to A, or newest first
    //   formats     only books in these formats; empty for all
    //   localOnly   list the local books (see setLocalBooks) instead of asking the server,
    //               so it works offline
    Q_INVOKABLE void setBookQuery(const QString &sortField, bool descending,
                                  const QStringList &formats, bool localOnly = false);
    // Reloads the book list from page one
    Q_INVOKABLE void refreshBooks();
    // Loads whichever pages cover rows first to last and aren't loaded yet. Called while
    // scrolling for the visible rows plus a screen either side.
    Q_INVOKABLE void loadBooks(int first, int last);

    // Searches titles, authors and series. Text shorter than MIN_SEARCH_LENGTH clears the
    // results.
    Q_INVOKABLE void search(const QString &text);
    // loadBooks() for the search results
    Q_INVOKABLE void loadSearchResults(int first, int last);
    // Fills booksOf with the books of some authors (field "author", values are names), a
    // series ("series", its name) or a collection ("collection", its id). Sorting and
    // filtering work as in setBookQuery(), plus "seriesIndex" (series order, with a section
    // per number) and, for collections, "collectionOrder" (order added). Existing rows stay
    // until the new ones arrive.
    Q_INVOKABLE void showBooksOf(const QString &field, const QStringList &values,
                                 const QString &sortField, bool descending,
                                 const QStringList &formats = {}, bool localOnly = false);
    // loadBooks() for booksOf
    Q_INVOKABLE void loadBooksOf(int first, int last);
    // The books on the device, as Downloads.downloadedBooks() lists them, for the lists
    // set to local books only. Offline, the series, authors and search are built from
    // these. Updates the lists showing them.
    Q_INVOKABLE void setLocalBooks(const QVariantList &books);
    // Fetches the user's collections, then each one's books for covers, authors and
    // formats. Existing rows stay until the new ones arrive.
    Q_INVOKABLE void loadCollections();
    // Sorts collections by "name" or "bookCount"
    Q_INVOKABLE void setCollectionSort(const QString &field, bool descending);
    // Fetches the library's series, then their books' formats in the background. Existing
    // rows stay until the new ones arrive.
    Q_INVOKABLE void loadSeries();
    // Sorts series by "name", "bookCount", "lastAddedAt" or "readProgress"
    Q_INVOKABLE void setSeriesSort(const QString &field, bool descending);
    // Fetches the library's authors, then their books in the background for formats and
    // counts. Existing rows stay until the new ones arrive.
    Q_INVOKABLE void loadAuthors();
    // Sorts authors by "name", "sortName", "bookCount" or "lastAddedAt", and keeps only
    // those with books in formats (and, with localOnly, among the local books).
    Q_INVOKABLE void setAuthorQuery(const QString &sortField, bool descending, const QStringList &formats,
                                    bool localOnly = false);
    // Fills authorSeriesList with the series containing books by author, then looks up
    // their formats
    Q_INVOKABLE void showAuthorSeries(const QString &author);
    // Same fields as setSeriesSort()
    Q_INVOKABLE void setAuthorSeriesSort(const QString &field, bool descending);
    // The author's unfiltered row from authorsList, or empty if not listed
    Q_INVOKABLE QVariantMap authorRow(int authorId) const;
    // Placeholder for a missing author photo, as BookOrbit draws it: initials ("AT" for
    // "Adrian Tchaikovsky") on a gradient chosen from the name, near the accent hue.
    // Returns {initials, from, to, color}, saturated for e-ink.
    Q_INVOKABLE QVariantMap authorMonogram(const QString &name) const;
    // Fetches an author's details and emits authorDetails(), with empty details on failure
    Q_INVOKABLE void loadAuthorDetails(int authorId);
    // Fetches a book's full details and emits bookDetails(), with empty details on failure
    Q_INVOKABLE void loadBookDetails(int bookId);

    // Adds text to the top of the recent searches (moving it if already there, ignoring
    // case) and saves them
    Q_INVOKABLE void addRecentSearch(const QString &text);

    // Authenticated GET for raw data such as thumbnails. Doesn't set busy.
    void download(const QString &path, DataHandler onSuccess, ErrorHandler onError);
    // Authenticated GET for a large file the caller streams. Refreshes the access token
    // first so it outlasts the download. The caller owns the reply.
    void startStream(const QString &path, std::function<void(QNetworkReply *)> onStarted,
                     ErrorHandler onError);
    // Authenticated background request. Errors go to onError, not the error property, and
    // it doesn't set busy.
    void request(const QByteArray &method, const QString &path, const QJsonDocument &body,
                 DataHandler onSuccess, ErrorHandler onError);
    // Calls then with the library id, fetching it first if needed
    void withLibrary(std::function<void(int libraryId)> then, ErrorHandler onError);

signals:
    void serverUrlChanged();
    void usernameChanged();
    void signedInChanged();
    void busyChanged();
    void onlineChanged();
    void errorChanged();
    void booksChanged();
    void jumpSectionsChanged();
    void searchChanged();
    void booksOfChanged();
    void collectionsChanged();
    void seriesListChanged();
    void authorsChanged();
    void authorSeriesChanged();
    void authorDetails(int authorId, const QVariantMap &details);
    void bookDetails(int bookId, const QVariantMap &details);
    void recentSearchesChanged();
    void accentChanged();

private:
    // How a list is sectioned: not at all, A-Z (from the server's jump buckets), by day
    // (date sorts), or by series number (rows then omit the series name)
    enum class Sections { None, Letters, Days, SeriesNumbers };

    // A book list loaded a page at a time. The model has a row for every book once the
    // first page arrives; other pages load on demand.
    struct PagedBooks {
        BookListModel model;
        // BookOrbit query: sort, filter and search text
        QJsonObject query;
        // Pages (from 0) loaded and in flight for the current query
        QSet<int> loadedPages;
        QSet<int> loadingPages;
        // Endpoint: the library's books or a collection's
        QString path;
        QString jumpPath;
        // Sort and filter (see setBookQuery), plus fixed rules such as booksOf's author or
        // series
        QJsonArray rules;
        QString sortField = "title";
        bool sortDescending = false;
        QStringList formats;
        bool onlyListed = false;
        QVariantList onlyBooks;
        // Bumped on reload so stale replies are dropped
        int queryGeneration = 0;
        // Generation the rows belong to. The old rows stay until a new query's first page
        // arrives.
        int listGeneration = -1;
        int total = 0;
        // Hide audiobook records whose ebook is a separate record, so one row covers both.
        // Only safe when the ebook is guaranteed to be in the list.
        bool mergePairs = false;
        Sections sections = Sections::None;
        // See jumpSections. For Letters, sectionStarts maps each section's first row to its
        // letter.
        QVariantList jumpSections;
        QHash<int, QString> sectionStarts;
    };

    // Collections, series or authors: one row each, sorted locally
    struct Groups {
        // rows is in server order, model is sorted
        QList<QVariantMap> rows;
        BookListModel model;
        // Letter sections, when sorted by name
        QVariantList jumpSections;
        bool loaded = false;
        // Bumped on reload so stale replies are dropped
        int generation = 0;
        QString sort = "name";   // the sort menu's default
        bool descending = false;
        // Authors' filter (see setAuthorQuery)
        QStringList formats;
        bool localOnly = false;
        // Whether every row's books have been looked up (series and authors only)
        bool booksLoaded = false;
        // Rows built from the local books while offline, to fetch again once online
        bool local = false;
    };

    // Endpoint for library book queries
    QString libraryBooksPath() const { return QString("/libraries/%1/books").arg(m_libraryId); }
    // Loads a collection's books from page onwards into its row
    void loadCollectionBooks(int collectionId, int page, int generation);
    // Sorts rows into the model and builds the A-Z sections
    void showGroups(Groups &groups);
    // Loads series (the library's or an author's) one page at a time
    void loadSeriesPage(Groups *groups, int page, int generation);
    // Looks up books for the rows from first onwards, a request at a time, for formats
    // and counts
    void loadGroupBooks(Groups *groups, int first, int page, int generation);
    // Loads authors one page at a time
    void loadAuthorsPage(int page, int generation);
    // Offline: rows for the series (the library's or the author's) or authors of the local
    // books, as though looked up
    void showLocalSeries(Groups &groups);
    void showLocalAuthors();
    // Offline: the local books matching the search text
    void searchLocalBooks(const QString &text);
    // On the device, connects Wi-Fi if needed: with the system's dialog if ask, otherwise
    // silently. Returns whether it's connected.
    bool ensureNetwork(bool ask);
    // Refreshes the session to see whether the server can be reached
    void checkServer();
    // Fails a request without sending it
    void failOffline(const ErrorHandler &onError);
    void setOnline(bool online);
    // withToken adds the access token (login, refresh and logout don't use it). Quiet
    // requests don't set busy.
    void send(const QByteArray &method, const QString &path, const QJsonDocument &body,
              DataHandler onSuccess, ErrorHandler onError, bool withToken = false,
              bool quiet = false);
    // Sends with the access token, refreshing and retrying once on a 401
    void call(const QByteArray &method, const QString &path, const QJsonDocument &body,
              DataHandler onSuccess, ErrorHandler onError, bool quiet = false,
              bool retried = false);
    // call() for JSON replies. Errors set the error property, then call onFailed.
    void callJson(const QByteArray &method, const QString &path, const QJsonDocument &body,
                  std::function<void(const QJsonDocument &)> onSuccess,
                  std::function<void()> onFailed = nullptr);
    // Reads the accent colour from the user's synced theme, if any
    void loadAccent();
    // Fetches the library id, then calls then
    void loadLibrary(std::function<void()> then);
    // Builds the BookOrbit query for a list's sort and filter
    static QJsonObject bookQuery(const PagedBooks &list);
    // Reloads from page one, or lists the local books
    void refreshList(PagedBooks &list);
    // The local books among booksOf's author, series or collection
    QVariantList localBooksOf() const;
    // Filters, sorts and sections onlyBooks locally, matching the server's lists
    void showListedBooks(PagedBooks &list);
    // Loads the A-Z jump buckets for a list's query, then calls then
    void loadJumpSections(PagedBooks &list, std::function<void()> then);
    // Loads series numbers for a list sorted by them, as its jumpSections, from page
    // onwards
    void loadSeriesSections(PagedBooks &list, int page = 0);
    // Clears a list's jump sections
    void clearJumpSections(PagedBooks &list);
    // Starts a new query generation, so replies to the old one are ignored
    void restartPages(PagedBooks &list);
    void loadPages(PagedBooks &list, int first, int last);
    void loadPage(PagedBooks &list, int page);
    // Finds other records of the same books on a newly loaded page (e.g. a separate
    // audiobook with the same title and first author) and merges their formats in
    void lookUpCompanions(PagedBooks *list, int first, const QVariantList &books, int generation);
    // Adds display fields to a row: section, sectionLabel, statusText, seriesText, and
    // variants, formats and hidden (see addCompanions)
    QVariantMap decorateBook(const QVariantMap &book, const PagedBooks &list) const;
    // Merges other records of the same book into a row's variants and formats. With
    // hideAudio, hides an audiobook row whose ebook is also listed.
    static void addCompanions(QVariantMap &row, const QList<QVariantMap> &companions, bool hideAudio);
    // Marks which rows from first to last start a section
    void updateSectionStarts(PagedBooks &list, int first, int last);
    // Letter sorts and date sorts
    static bool isLetterSort(const QString &field) { return field == "title" || field == "author"; }
    static bool isDateSort(const QString &field) { return field == "addedAt" || field == "lastReadAt"; }
    // Exchanges the refresh token for a new access token, then runs any queued callbacks
    void refreshSession(std::function<void()> then);
    void applyCredentials(const QJsonObject &credentials);
    void clearSession();
    void setSignedIn(bool signedIn);
    void setError(const QString &error);

    Database *m_database;
    QNetworkAccessManager m_nam;
    QString m_serverUrl;
    QString m_username;
    QString m_accessToken;
    QString m_refreshToken;
    bool m_signedIn = false;
    bool m_online = true;
    // PBOOKORBIT_OFFLINE=1: every request fails as though there were no network
    bool m_simulateOffline = false;
    int m_pending = 0;
    QString m_error;
    int m_libraryId = -1;
    // Main library list
    PagedBooks m_list;
    PagedBooks m_search;
    PagedBooks m_booksOf;
    // What booksOf shows (see showBooksOf). For a collection, the book ids
    // loadCollections() found, newest first.
    QString m_booksOfField;
    QStringList m_booksOfValues;
    QVariantList m_booksOfCollectionIds;
    Groups m_collections;
    Groups m_series;
    Groups m_authors;
    Groups m_authorSeries;
    // Author whose series m_authorSeries holds
    QString m_authorSeriesName;
    QStringList m_recentSearches;
    QVariantList m_localBooks;
    QString m_accent;   // BookOrbit accent name, e.g. "teal"; empty for the default
    bool m_refreshing = false;
    QList<std::function<void()>> m_afterRefresh;
};
