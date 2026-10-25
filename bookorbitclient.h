#pragma once

#include "booklistmodel.h"

#include <QColor>
#include <QJsonArray>
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
    // The user's book collections, as loadCollections() lists them, one row each (role
    // "book", as in the book lists): {id, name, icon (a Lucide icon name or "custom:<slug>"),
    // bookCount, covers: [{id, coverVersion}] (the last two books added to it that have
    // covers), authors (of all its books, each once, by surname), formats (of all its
    // books), bookIds (latest added first)}. These fill in as its books load, and then
    // bookCount counts an ebook and its audiobook kept as separate records once, as its
    // book list shows them.
    Q_PROPERTY(BookListModel *collections READ collections CONSTANT)
    // jumpSections for collections: their letters when sorted by name, else empty. Each
    // row starting one has sectionStart and sectionLabel, as in the book lists.
    Q_PROPERTY(QVariantList collectionJumpSections READ collectionJumpSections NOTIFY collectionsChanged)
    // Whether the collections have been listed since signing in
    Q_PROPERTY(bool collectionsLoaded READ collectionsLoaded NOTIFY collectionsChanged)
    // The library's series, as loadSeries() lists them, in rows like the collections':
    // {id, name, bookCount, covers (its first two books, in its order), authors (by
    // surname), formats (of all its books, filled in later), lastAddedAt, readProgress
    // (the share of its books read, 0 to 1)}, bookCount counting an ebook and its audiobook
    // once when the formats are in; and as for the collections, the letters to
    // jump to when sorted by name, and whether they have been listed
    Q_PROPERTY(BookListModel *seriesList READ seriesList CONSTANT)
    Q_PROPERTY(QVariantList seriesJumpSections READ seriesJumpSections NOTIFY seriesListChanged)
    Q_PROPERTY(bool seriesLoaded READ seriesLoaded NOTIFY seriesListChanged)
    // The library's authors, as loadAuthors() lists them, in rows like the series': {kind:
    // "author", id, name, sortName ("Martin, George R.R."), bookCount, photo (the authors
    // list's imageUrl, or "" without one), lastAddedAt, formats (of their books, filled in
    // later)}, bookCount counting an ebook and its audiobook once when their books are in;
    // under a filter, the count and formats are of just the books it lets through. As for
    // the series, the letters to jump to when sorted by name or sort
    // name, and whether they have been listed
    Q_PROPERTY(BookListModel *authorsList READ authorsList CONSTANT)
    Q_PROPERTY(QVariantList authorJumpSections READ authorJumpSections NOTIFY authorsChanged)
    Q_PROPERTY(bool authorsLoaded READ authorsLoaded NOTIFY authorsChanged)
    // Whether every author's books have been looked up, which their filter needs
    Q_PROPERTY(bool authorBooksLoaded READ authorBooksLoaded NOTIFY authorsChanged)
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
    // Lists in booksOf every book by any of the authors (field "author", values their
    // names), in the series (field "series", values its name) or in a collection (field
    // "collection", values its id), sorted and filtered as setBookQuery() does the
    // library. Two more sort fields: "seriesIndex", the series' order, each book under its
    // number; and for a collection "collectionOrder", the order they were added to it
    // (descending: the latest first). A collection's downloaded books are listed if they
    // are among its books that loadCollections() found. Called again for the same books,
    // the old rows stay until the new ones arrive.
    Q_INVOKABLE void showBooksOf(const QString &field, const QStringList &values,
                                 const QString &sortField, bool descending,
                                 const QStringList &formats = {}, bool onlyListed = false,
                                 const QVariantList &onlyBooks = {});
    // loadBooks() for booksOf
    Q_INVOKABLE void loadBooksOf(int first, int last);
    // Lists the user's book collections (GET /collections) in collections, then loads each
    // one's books for its covers, authors and formats. The old rows stay until the new
    // ones arrive.
    Q_INVOKABLE void loadCollections();
    // How collections is sorted: field "name" or "bookCount"
    Q_INVOKABLE void setCollectionSort(const QString &field, bool descending);
    // Lists the library's series (GET /series) in seriesList, then looks up their books'
    // formats in the background. The old rows stay until the new ones arrive.
    Q_INVOKABLE void loadSeries();
    // How seriesList is sorted: field "name", "bookCount", "lastAddedAt" or "readProgress"
    Q_INVOKABLE void setSeriesSort(const QString &field, bool descending);
    // Lists the library's authors (GET /authors) in authorsList, then looks up their books
    // in the background, for their formats and counts. The old rows stay until the new
    // ones arrive.
    Q_INVOKABLE void loadAuthors();
    // How authorsList is sorted ("name", "sortName", "bookCount" or "lastAddedAt") and
    // filtered, worked out from their books as setBookQuery() filters the library: only
    // authors with books in one of formats, or with downloadedOnly, with such books among
    // downloadedBooks (as Downloads.downloadedBooks() gives them)
    Q_INVOKABLE void setAuthorQuery(const QString &sortField, bool descending, const QStringList &formats,
                                    bool downloadedOnly = false, const QVariantList &downloadedBooks = {});
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
    void collectionsChanged();
    void seriesListChanged();
    void authorsChanged();
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
        // Where the query goes: the library's books, or a collection's
        QString path;
        QString jumpPath;
        // For the library and booksOf, the sort and filter (see setBookQuery), and filter
        // rules the list always has: booksOf's author or series
        QJsonArray rules;
        QString sortField = "title";
        bool sortDescending = false;
        QStringList formats;
        bool onlyListed = false;
        QVariantList onlyBooks;
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

    // A list of groups of books, the collections or the series: one row each, sorted on
    // the device
    struct Groups {
        // In the server's order, and as listed, sorted
        QList<QVariantMap> rows;
        BookListModel model;
        // The letters to jump to, when sorted by name
        QVariantList jumpSections;
        bool loaded = false;
        // Bumped by every reload, so replies to an older one are dropped
        int generation = 0;
        QString sort = "name";   // matches the sort menus' default, Name A to Z
        bool descending = false;
        // For the authors, the filter (see setAuthorQuery)
        QStringList formats;
        bool downloadedOnly = false;
        QVariantList downloadedBooks;
        // Whether every row's books have been looked up (series and authors)
        bool booksLoaded = false;
    };

    // Where the library's book queries go
    QString libraryBooksPath() const { return QString("/libraries/%1/books").arg(m_libraryId); }
    // Loads a collection's books from page on, adding them to its row
    void loadCollectionBooks(int collectionId, int page, int generation);
    // Fills a list of groups' model from its rows, sorted, in A-Z sections by name
    void showGroups(Groups &groups);
    // Loads a page of series, then the rest
    void loadSeriesPage(int page, int generation);
    // Looks up the books of the series or authors from row first on, a request's worth
    // at a time (page for a request's later pages), for their formats and counts
    void loadGroupBooks(Groups *groups, int first, int page, int generation);
    // Loads a page of authors, then the rest
    void loadAuthorsPage(int page, int generation);
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
    // A list's sort, and its filter if there is one, as BookOrbit's book queries take them
    static QJsonObject bookQuery(const PagedBooks &list);
    // Reloads the library or a collection's books (with its path set) from the first page,
    // or lists its onlyBooks
    void refreshList(PagedBooks &list);
    // Fills a list with its onlyBooks: filtered by the downloaded file's format, sorted, and
    // split into sections like the server's lists
    void showListedBooks(PagedBooks &list);
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
    // and variants, formats and hidden (see addCompanions), for a row of list
    QVariantMap decorateBook(const QVariantMap &book, const PagedBooks &list) const;
    // Adds other records of the same book to a row: their versions to its variants and
    // their formats to its formats. Hides the row (an audiobook record whose ebook is
    // listed too) if hideAudio.
    static void addCompanions(QVariantMap &row, const QList<QVariantMap> &companions, bool hideAudio);
    // Works out which of a list's rows first to last start a section, so they show its box
    void updateSectionStarts(PagedBooks &list, int first, int last);
    // The sorts split into A-Z sections, and those by day
    static bool isLetterSort(const QString &field) { return field == "title" || field == "author"; }
    static bool isDateSort(const QString &field) { return field == "addedAt" || field == "lastReadAt"; }
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
    PagedBooks m_search;
    PagedBooks m_booksOf;
    // What booksOf lists (see showBooksOf), and for a collection its books as far as
    // loadCollections() found them, latest added first
    QString m_booksOfField;
    QStringList m_booksOfValues;
    QVariantList m_booksOfCollectionIds;
    Groups m_collections;
    Groups m_series;
    Groups m_authors;
    QStringList m_recentSearches;
    QString m_accent;   // a BookOrbit accent name, e.g. "teal"; empty for the default
    bool m_refreshing = false;
    QList<std::function<void()>> m_afterRefresh;
};
