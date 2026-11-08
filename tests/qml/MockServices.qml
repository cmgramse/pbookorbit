import QtQuick

// Stands in for the C++ objects main.cpp gives the app, so main.qml's wiring can run with no
// warnings. One object plays them all, with empty lists.
QtObject {
    // BookOrbitClient
    property bool signedIn: true
    property bool online: true
    property bool busy: false
    property bool booksLoaded: true
    property string error: ""
    property string serverUrl: ""
    property string suggestedServer: ""
    property string username: ""
    property color accentColor: "teal"
    property color accentTextColor: "white"
    property var libraries: [{id: 1, label: "Alpha"}, {id: 2, label: "Beta"}]
    property int currentLibraryId: 1
    property string currentLibraryLabel: "Alpha"
    property var recentSearches: []
    property var jumpSections: []
    property var booksOfJumpSections: []
    property var collectionJumpSections: []
    property var seriesJumpSections: []
    property var authorJumpSections: []
    property var authorSeriesJumpSections: []
    property int searchTotal: 0
    property int booksOfTotal: 0
    property bool collectionsLoaded: true
    property bool seriesLoaded: true
    property bool authorsLoaded: true
    property bool authorBooksLoaded: true
    property bool authorSeriesLoaded: true
    property var genreJumpSections: []
    property bool genresLoaded: true
    property string genresStatus: "idle"
    property int genresProgress: 0
    property int genresTotal: -1
    property bool genresCached: false
    property ListModel genresList: ListModel {}
    // What the genre tests look at
    property int genreLoads: 0
    property var genreSort: []
    property var booksOfShown: []
    property ListModel books: ListModel {}
    property ListModel searchResults: ListModel {}
    property ListModel booksOf: ListModel {}
    property ListModel collections: ListModel {}
    property ListModel seriesList: ListModel {}
    property ListModel authorsList: ListModel {}
    property ListModel authorSeriesList: ListModel {}
    signal librarySwitched()
    signal libraryFallback()
    signal authorDetails(int authorId, var details)
    signal bookDetails(int bookId, var details)
    signal authorsChanged()
    signal searchChanged()
    signal authorSeriesChanged()
    signal savedBooksChanged()
    function start() {}
    function setLocalBooks(books) {}
    function setBookQuery() {}
    function setAuthorQuery() {}
    function setCollectionSort() {}
    function setSeriesSort() {}
    function setAuthorSeriesSort() {}
    function refreshLibraries() {}
    function loadSeries() {}
    function loadGenres() { genreLoads++ }
    function setGenreSort(field, descending) { genreSort = [field, descending] }
    function showBooksOf(field, values) { booksOfShown = [field, values] }
    function loadAuthors() {}
    function loadCollections() {}
    function loadBooks() {}
    function loadBooksOf() {}
    function loadSearchResults() {}
    function authorRow() { return ({}) }
    function authorMonogram() { return {initials: "", from: "white", to: "white", color: "black"} }

    // Downloads
    property int revision: 0
    property int percent: 0
    property int bookId: -1
    signal changed()
    signal downloaded()
    function downloadedBooks() { return [] }
    function isOnDevice() { return false }
    function formatsOnDevice() { return [] }
    function kindsOnDevice() { return [] }
    function pathsOnDevice() { return [] }

    // Covers
    signal ready(int bookId)
    signal photoReady(string url)
    function source() { return "" }
    function authorPhoto() { return "" }

    // BookFinder
    property string state: "idle"
    property int foundCount: 0
    property int notFoundCount: 0
    property var unsure: []
    function shouldOffer() { return false }
    function unrecordedCount() { return 0 }

    // Browser, Eink, Log, LauncherTile
    property bool available: false
    property bool enabled: false
    property bool added: true
    function enterA2() {}
    function leaveA2() {}
    function setDynamicUpdates() {}
    function requireHqUpdate() {}
}
