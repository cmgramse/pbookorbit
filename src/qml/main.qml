import QtQuick
import QtQuick.Window
import "units.js" as Units
import "night.js" as Night

Window {
    id: root
    visible: true
    width: screenWidth
    height: windowHeight
    color: "white"

    // Faster e-ink updates while typing. callLater because moving focus between fields
    // briefly leaves neither focused.
    readonly property bool typing: (loginPage.visible && loginPage.typing)
                                   || (searchPage.visible && searchPage.typing)
    onTypingChanged: Qt.callLater(() => eink.setDynamicUpdates(root.typing))

    // Search page, over everything else
    property bool searching: false
    // Pages opened from the book menu, a collection or an author, stacked over the search
    // page: author info, then their series, then a books-of list, then book info
    property bool showingAuthor: false
    property bool showingAuthorSeries: false
    property bool showingBooksOf: false
    property bool showingInfo: false

    function closePages() {
        bookMenu.close()
        showingInfo = false
        showingBooksOf = false
        showingAuthorSeries = false
        showingAuthor = false
        searching = false
    }

    function openSearch() {
        menu.visible = false
        searching = true
        searchPage.open()
    }

    // Opens a downloaded book in its reader (the ebook if both versions are there).
    // Otherwise offers to download it, asking which version if there's an ebook and an
    // audiobook.
    function openBook(book) {
        const variants = book.variants || []
        for (const variant of variants) {
            if (downloads.isOnDevice(variant.bookId)) {
                downloads.open(variant.bookId)
                return
            }
        }
        if (downloads.isOnDevice(book.id))
            downloads.open(book.id)
        else
            downloadDialog.show(book, variants)
    }

    // Tapping the title scrolls the current list back to the top
    function backToTop() {
        menu.visible = false
        closeDropdowns()
        activeList.positionViewAtBeginning()
    }

    // Footer views: Library, Collections, Series or Authors
    readonly property bool showingCollections: footer.currentIndex === 1
    readonly property bool showingSeries: footer.currentIndex === 2
    readonly property bool showingAuthors: footer.currentIndex === 3
    // The list whose sub-toolbar menus are in use
    readonly property ListView activeList: showingBooksOf ? booksOfPage.list
                                           : showingAuthorSeries ? authorSeriesPage.list
                                           : showingCollections ? collectionsPage.list
                                           : showingSeries ? seriesPage.list
                                           : showingAuthors ? authorsPage.list : bookList

    AppHeader {
        id: header
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        // "Orbit" in the user's BookOrbit accent colour
        title: "PocketBook × Book<font color=\"" + Night.color(bookOrbit.accentColor) + "\">Orbit</font>"
        menuOpen: menu.visible
        showOffline: bookOrbit.signedIn && !bookOrbit.online
        showSearch: bookOrbit.signedIn
        // Only has Sign out
        showMenu: bookOrbit.signedIn
        onHomeClicked: Qt.quit()
        onTitleClicked: root.backToTop()
        onSecretTapped: {
            diagnostics.toggle()
            diagnosticsDialog.visible = true
        }
        onOfflineClicked: bookOrbit.goOnline()
        onSearchClicked: root.openSearch()
        onMenuClicked: menu.visible = !menu.visible
    }

    // Signed out
    LoginPage {
        id: loginPage
        visible: !bookOrbit.signedIn
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
    }

    // Signed in: sub-toolbar with filter and sort, the book list, and a footer for the
    // views, modelled on Libby
    readonly property var filterOptions: ["All titles", "Ebooks", "Audiobooks"]
    // File formats per filter option (BookOrbit's EBOOK_FORMAT_LIST and AUDIO_FORMAT_LIST)
    readonly property var filterFormats: [
        [],
        ["epub", "kepub", "mobi", "azw3", "azw", "fb2", "pdf", "djvu"],
        ["m4b", "mp3", "m4a", "opus", "ogg", "flac"]
    ]

    // Library sort and filter. Dates default to newest first, everything else to A-Z.
    ListQuery {
        id: libraryQuery
        sortOptions: ["Date added", "Title", "Author", "Recently read"]
        sortFields: ["addedAt", "title", "author", "lastReadAt"]
        sortDescendingByDefault: [true, false, false, true]
        sortIndex: 1   // Title, matching bookOrbit's default
        onApplied: {
            bookOrbit.setBookQuery(sortFields[sortIndex], descending(), root.filterFormats[filterIndex],
                                   downloadedOnly, downloadedOnly ? downloads.downloadedBooks() : [])
            bookList.positionViewAtBeginning()
        }
    }
    // Collection sorts from BookOrbit's collections page, minus "Custom order": A-Z, and
    // most books first
    ListQuery {
        id: collectionsQuery
        sortOptions: ["Name", "Book count"]
        sortFields: ["name", "bookCount"]
        sortDescendingByDefault: [false, true]
        hasFilter: false
        onApplied: {
            bookOrbit.setCollectionSort(sortFields[sortIndex], descending())
            collectionsPage.list.positionViewAtBeginning()
        }
    }
    // Series sorts from BookOrbit's series page: A-Z, then most books, latest added and
    // most read first
    ListQuery {
        id: seriesQuery
        sortOptions: ["Name", "Book count", "Recently added", "Read progress"]
        sortFields: ["name", "bookCount", "lastAddedAt", "readProgress"]
        sortDescendingByDefault: [false, true, true, true]
        hasFilter: false
        onApplied: {
            bookOrbit.setSeriesSort(sortFields[sortIndex], descending())
            seriesPage.list.positionViewAtBeginning()
        }
    }
    // Author sorts from BookOrbit's authors page: A-Z by name or surname, most books and
    // latest added first. Filtered by their books, like the library.
    ListQuery {
        id: authorsQuery
        sortOptions: ["Name", "Sort name", "Book count", "Recently added"]
        sortFields: ["name", "sortName", "bookCount", "lastAddedAt"]
        sortDescendingByDefault: [false, false, true, true]
        onApplied: {
            bookOrbit.setAuthorQuery(sortFields[sortIndex], descending(), root.filterFormats[filterIndex],
                                     downloadedOnly, downloadedOnly ? downloads.downloadedBooks() : [])
            authorsPage.list.positionViewAtBeginning()
        }
    }
    // An author's series: the Series view's sorts, kept between authors
    ListQuery {
        id: authorSeriesQuery
        sortOptions: seriesQuery.sortOptions
        sortFields: seriesQuery.sortFields
        sortDescendingByDefault: seriesQuery.sortDescendingByDefault
        hasFilter: false
        onApplied: {
            bookOrbit.setAuthorSeriesSort(sortFields[sortIndex], descending())
            authorSeriesPage.list.positionViewAtBeginning()
        }
    }
    function openAuthorSeries(name) {
        authorSeriesView.name = name
        bookOrbit.showAuthorSeries(name)
        authorSeriesPage.list.positionViewAtBeginning()
        showingAuthorSeries = true
    }
    // Books of an author, series or collection (see bookOrbit.showBooksOf()). Each kind
    // keeps its own sort and filter between uses, like the library.
    property string booksOfField
    property var booksOfValues: []
    readonly property ListQuery booksOfQuery: booksOfField === "series" ? seriesBooksQuery
                                              : booksOfField === "collection" ? collectionBooksQuery
                                              : authorBooksQuery
    function openBooksOf(kind, name, field, values) {
        booksOfPage.kind = kind
        booksOfPage.name = name
        booksOfField = field
        booksOfValues = values
        applyBooksOf(booksOfQuery)
        showingBooksOf = true
    }
    // Used by the book menu's "All books of" items and the book info page's links. field is
    // "author" or "series".
    function openBooksOfChosen(field, values, title) {
        openBooksOf(field === "series" ? "Series" : values.length > 1 ? "Authors" : "Author", title, field, values)
    }
    function applyBooksOf(query) {
        bookOrbit.showBooksOf(booksOfField, booksOfValues, query.sortFields[query.sortIndex], query.descending(),
                              filterFormats[query.filterIndex], query.downloadedOnly,
                              query.downloadedOnly ? downloads.downloadedBooks() : [])
        booksOfPage.list.positionViewAtBeginning()
    }
    // An author's books use the library's sorts
    ListQuery {
        id: authorBooksQuery
        sortOptions: libraryQuery.sortOptions
        sortFields: libraryQuery.sortFields
        sortDescendingByDefault: libraryQuery.sortDescendingByDefault
        sortIndex: 1
        onApplied: root.applyBooksOf(authorBooksQuery)
    }
    // A series' books: series order first, then the library's sorts
    ListQuery {
        id: seriesBooksQuery
        sortOptions: ["Series order"].concat(libraryQuery.sortOptions)
        sortFields: ["seriesIndex"].concat(libraryQuery.sortFields)
        sortDescendingByDefault: [false].concat(libraryQuery.sortDescendingByDefault)
        onApplied: root.applyBooksOf(seriesBooksQuery)
    }
    // A collection's books: the library's sorts, with Date added meaning when it was added
    // to the collection
    ListQuery {
        id: collectionBooksQuery
        sortOptions: libraryQuery.sortOptions
        sortFields: ["collectionOrder", "title", "author", "lastReadAt"]
        sortDescendingByDefault: libraryQuery.sortDescendingByDefault
        sortIndex: 1
        onApplied: root.applyBooksOf(collectionBooksQuery)
    }
    // Downloads (and details lookups) change the downloaded book list
    readonly property int downloadsRevision: downloads.revision
    onDownloadsRevisionChanged: {
        bookOrbit.setLocalBooks(downloads.downloadedBooks())
        if (libraryQuery.downloadedOnly)
            libraryQuery.applied()
        if (showingBooksOf && booksOfQuery.downloadedOnly)
            booksOfQuery.applied()
        if (authorsQuery.downloadedOnly)
            authorsQuery.applied()
    }

    // Going offline or back online changes what lists that aren't downloaded only show
    Connections {
        target: bookOrbit
        function onOnlineChanged() {
            if (!libraryQuery.downloadedChosen)
                libraryQuery.applied()
            if (showingBooksOf && !booksOfQuery.downloadedChosen)
                booksOfQuery.applied()
            if (!authorsQuery.downloadedChosen)
                authorsQuery.applied()
            // Offline, series are the downloaded books'. Back online, rows built offline
            // count as not loaded.
            if (showingSeries && (!bookOrbit.seriesLoaded || !bookOrbit.online))
                bookOrbit.loadSeries()
            if (showingAuthorSeries)
                bookOrbit.showAuthorSeries(authorSeriesView.name)
            if (showingAuthors && !bookOrbit.authorsLoaded)
                bookOrbit.loadAuthors()
            if (showingCollections && bookOrbit.online)
                bookOrbit.loadCollections()
        }
    }
    Component.onCompleted: {
        bookOrbit.setLocalBooks(downloads.downloadedBooks())
        // Starting offline: list the downloaded books before bookOrbit.start() loads the
        // list
        if (!bookOrbit.online)
            libraryQuery.applied()
    }

    // The sort and filter shown by the menus, for whichever page is in front
    readonly property ListQuery activeQuery: showingBooksOf ? booksOfQuery
                                             : showingAuthorSeries ? authorSeriesQuery
                                             : showingCollections ? collectionsQuery
                                             : showingSeries ? seriesQuery
                                             : showingAuthors ? authorsQuery : libraryQuery
    readonly property bool dropdownOpen: filterMenu.visible || sortMenu.visible

    // The page-turn keys (sent as Page Up/Page Down) page the front list or info page, like
    // its page buttons. Not on the sign-in page, and not while a menu or dialog is open,
    // since a tap would only close it.
    readonly property PageButtons frontPageButtons: showingInfo ? bookInfoPage.pageButtons
                                                    : showingBooksOf ? booksOfPage.pageButtons
                                                    : showingAuthorSeries ? authorSeriesPage.pageButtons
                                                    : showingAuthor ? authorInfoPage.pageButtons
                                                    : searching ? searchPage.pageButtons
                                                    : !bookOrbit.signedIn ? null
                                                    : showingCollections ? collectionsPage.pageButtons
                                                    : showingSeries ? seriesPage.pageButtons
                                                    : showingAuthors ? authorsPage.pageButtons
                                                    : pageButtons
    readonly property bool overlayOpen: dropdownOpen || menu.visible || bookMenu.visible
                                        || deleteDialog.visible || downloadDialog.visible
                                        || findBooksDialog.visible

    function pageFront(direction) {
        if (frontPageButtons && frontPageButtons.visible && !overlayOpen)
            frontPageButtons.page(direction)
    }

    Shortcut {
        sequence: "PgUp"
        onActivated: root.pageFront(-1)
    }
    Shortcut {
        sequence: "PgDown"
        onActivated: root.pageFront(1)
    }

    function closeDropdowns() {
        filterMenu.visible = false
        sortMenu.visible = false
    }

    function toggleDropdown(dropdown) {
        const open = !dropdown.visible
        closeDropdowns()
        if (open) {
            // Bring a partly scrolled-off bar fully into view first
            if (activeList.headerItem.mapToItem(root.contentItem, 0, 0).y < activeList.y)
                activeList.positionViewAtBeginning()
            // Align the menu and the tab copy with where the sub-toolbar has scrolled to.
            // The list can't move while a menu is open, since the fade takes taps.
            openTab.y = activeList.headerItem.mapToItem(root.contentItem, 0, 0).y
            dropdown.visible = true
        }
    }

    ListView {
        id: bookList
        visible: bookOrbit.signedIn && !root.showingCollections && !root.showingSeries && !root.showingAuthors
        anchors.top: header.bottom
        anchors.bottom: footer.top
        anchors.left: parent.left
        anchors.right: parent.right
        clip: true
        // No bounce at the ends; on e-ink that's just extra redraws
        boundsBehavior: Flickable.StopAtBounds
        model: bookOrbit.books

        // Load the visible books plus a screen either side once scrolling pauses, so a fast
        // flick doesn't fetch every page it passes
        function loadVisibleBooks() {
            if (count === 0)
                return
            let first = indexAt(width / 2, contentY)
            let last = indexAt(width / 2, contentY + height - 1)
            if (first < 0)
                first = 0   // the sub-toolbar is at the top
            if (last < 0)
                last = count - 1
            const screenful = last - first + 1
            bookOrbit.loadBooks(first - screenful, last + screenful)
        }
        Timer {
            id: loadTimer
            interval: 150
            onTriggered: bookList.loadVisibleBooks()
        }
        onContentYChanged: loadTimer.restart()
        onCountChanged: loadTimer.restart()

        // The filter/sort bar and any status line scroll with the books. The first row sits
        // right under the bar, as in the system library.
        header: Column {
            width: ListView.view.width
            bottomPadding: status.visible ? Units.dp(8) : 0

            SubToolbar {
                width: parent.width
                filterText: root.filterOptions[libraryQuery.filterIndex]
                sortText: "By " + libraryQuery.sortOptions[libraryQuery.sortIndex]
                sortReversed: libraryQuery.sortReversed
                filterOpen: filterMenu.visible
                sortOpen: sortMenu.visible
                tabOverhang: filterMenu.border.width - 1
                onFilterClicked: root.toggleDropdown(filterMenu)
                onSortClicked: root.toggleDropdown(sortMenu)
                onSortIconClicked: libraryQuery.reverseSort()
            }
            Text {
                id: status
                x: Units.dp(26)
                width: parent.width - 2 * x
                // The spinner shows while the first books load
                visible: bookOrbit.error !== "" || (bookList.count === 0 && !bookOrbit.busy && libraryQuery.downloadedOnly)
                topPadding: Units.dp(8)
                text: bookOrbit.error !== "" ? bookOrbit.error
                      : "No downloaded books" + (libraryQuery.filterIndex === 0 ? "" : " of this type")
                font.pointSize: 8
                wrapMode: Text.Wrap
            }
        }

        // Fast black-and-white (A2) updates for the list while it scrolls. A2 takes screen
        // coordinates, hence mapToGlobal (as in the SDK's qml_test).
        onMovementStarted: {
            const pos = mapToGlobal(0, 0)
            eink.enterA2ForFlick(pos.x, pos.y, width, height)
        }
        // Fires when momentum stops, not when the finger lifts
        onMovementEnded: eink.leaveA2Soon()
        onVisibleChanged: if (!visible) eink.leaveA2()
        delegate: BookRow {
            id: bookRow
            required book
            onClicked: root.openBook(book)
            onHeld: bookMenu.open(bookRow)
        }
    }

    ListScrollbar {
        list: bookList
        bottomInset: pageButtons.height
    }
    // Only until the first books arrive; later pages load on scroll
    Spinner {
        list: bookList
        visible: bookList.visible && bookOrbit.error === "" && bookList.count === 0 && bookOrbit.busy
    }

    // For A-Z sorts. Starts below the sub-toolbar, clear of the sort button, and stays put
    // when the sub-toolbar scrolls away.
    JumpRail {
        id: libraryRail
        list: bookList
        sections: bookOrbit.jumpSections
        descending: libraryQuery.sortDescending
        topInset: bookList.headerItem ? bookList.headerItem.height : 0
        bottomInset: pageButtons.height
    }

    // Over the bottom of the list, above the footer
    PageButtons {
        id: pageButtons
        list: bookList
        anchors.bottom: bookList.bottom
    }

    GroupsPage {
        id: collectionsPage
        visible: bookOrbit.signedIn && root.showingCollections
        model: bookOrbit.collections
        jumpSections: bookOrbit.collectionJumpSections
        loaded: bookOrbit.collectionsLoaded
        emptyText: "No collections"
        anchors.top: header.bottom
        anchors.bottom: footer.top
        anchors.left: parent.left
        anchors.right: parent.right
        sortText: "By " + collectionsQuery.sortOptions[collectionsQuery.sortIndex]
        sortReversed: collectionsQuery.sortReversed
        descending: collectionsQuery.sortDescending
        // Jump rail aligned and spaced like the library's
        jumpRailTop: libraryRail.entriesY - y
        jumpRailPitch: libraryRail.pitch
        sortOpen: sortMenu.visible
        tabOverhang: sortMenu.border.width - 1
        onSortClicked: root.toggleDropdown(sortMenu)
        onSortIconClicked: collectionsQuery.reverseSort()
        // Reloaded every time the view opens
        onVisibleChanged: if (visible) bookOrbit.loadCollections()
        onGroupClicked: (collection) => root.openBooksOf("Collection", collection.name, "collection",
                                                        [String(collection.id)])
    }

    GroupsPage {
        id: seriesPage
        visible: bookOrbit.signedIn && root.showingSeries
        model: bookOrbit.seriesList
        jumpSections: bookOrbit.seriesJumpSections
        loaded: bookOrbit.seriesLoaded
        emptyText: "No series"
        anchors.top: header.bottom
        anchors.bottom: footer.top
        anchors.left: parent.left
        anchors.right: parent.right
        sortText: "By " + seriesQuery.sortOptions[seriesQuery.sortIndex]
        sortReversed: seriesQuery.sortReversed
        descending: seriesQuery.sortDescending
        // Jump rail aligned and spaced like the library's
        jumpRailTop: libraryRail.entriesY - y
        jumpRailPitch: libraryRail.pitch
        sortOpen: sortMenu.visible
        tabOverhang: sortMenu.border.width - 1
        onSortClicked: root.toggleDropdown(sortMenu)
        onSortIconClicked: seriesQuery.reverseSort()
        // Loaded the first time the view opens: large libraries have many series, and their
        // formats take a request per few series. Offline, the downloaded books' each time.
        onVisibleChanged: if (visible && (!bookOrbit.seriesLoaded || !bookOrbit.online)) bookOrbit.loadSeries()
        onGroupClicked: (series) => root.openBooksOf("Series", series.name, "series", [series.name])
    }

    GroupsPage {
        id: authorsPage
        visible: bookOrbit.signedIn && root.showingAuthors
        model: bookOrbit.authorsList
        jumpSections: bookOrbit.authorJumpSections
        // With a filter on, authors appear once their books have been looked up
        loaded: bookOrbit.authorsLoaded && (bookOrbit.authorBooksLoaded
                                            || (authorsQuery.filterIndex === 0 && !authorsQuery.downloadedOnly))
        emptyText: authorsQuery.downloadedOnly ? "No authors with downloaded books"
                   : authorsQuery.filterIndex === 0 ? "No authors"
                   : "No authors with " + root.filterOptions[authorsQuery.filterIndex].toLowerCase()
        anchors.top: header.bottom
        anchors.bottom: footer.top
        anchors.left: parent.left
        anchors.right: parent.right
        hasFilter: true
        filterText: root.filterOptions[authorsQuery.filterIndex]
        filterOpen: filterMenu.visible
        sortText: "By " + authorsQuery.sortOptions[authorsQuery.sortIndex]
        sortReversed: authorsQuery.sortReversed
        descending: authorsQuery.sortDescending
        // Jump rail aligned and spaced like the library's
        jumpRailTop: libraryRail.entriesY - y
        jumpRailPitch: libraryRail.pitch
        sortOpen: sortMenu.visible
        tabOverhang: sortMenu.border.width - 1
        onFilterClicked: root.toggleDropdown(filterMenu)
        onSortClicked: root.toggleDropdown(sortMenu)
        onSortIconClicked: authorsQuery.reverseSort()
        // Loaded the first time the view opens, like the series
        onVisibleChanged: if (visible && !bookOrbit.authorsLoaded) bookOrbit.loadAuthors()
        onGroupClicked: (author) => {
            authorInfoPage.show(author.id)
            root.showingAuthor = true
        }
    }

    TabFooter {
        id: footer
        visible: bookOrbit.signedIn
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        // BookOrbit's views
        tabs: [
            { label: "Library", icon: "overdrive_library" },
            { label: "Collections", icon: "e3_filter_bookshelf" },
            { label: "Series", icon: "e3_filter_seria_bold" },
            { label: "Authors", icon: "e3_filter_author" }
        ]
        onTabClicked: (index) => {
            // Tapping the current tab again scrolls its list to the top
            if (index === currentIndex) {
                root.activeList.positionViewAtBeginning()
            } else {
                root.closeDropdowns()
                currentIndex = index
            }
        }
    }

    // While a filter or sort menu is open the window fades, as in Libby. Tapping elsewhere
    // closes it.
    Rectangle {
        anchors.fill: parent
        z: 1
        visible: root.dropdownOpen
        color: Qt.rgba(1, 1, 1, 0.6)
        MouseArea {
            anchors.fill: parent
            onClicked: root.closeDropdowns()
        }
    }
    // The open tab, drawn over the fade at the sub-toolbar's position
    SubToolbar {
        id: openTab
        visible: root.dropdownOpen
        z: 2
        width: parent.width
        onlyOpenTab: true
        showFilter: root.activeQuery.hasFilter
        filterText: root.filterOptions[root.activeQuery.filterIndex]
        sortText: "By " + root.activeQuery.sortOptions[root.activeQuery.sortIndex]
        sortReversed: root.activeQuery.sortReversed
        filterOpen: filterMenu.visible
        sortOpen: sortMenu.visible
        tabOverhang: filterMenu.border.width - 1
        onFilterClicked: root.closeDropdowns()
        onSortClicked: root.closeDropdowns()
    }
    PopupMenu {
        id: filterMenu
        visible: false
        z: 3
        x: openTab.filterMenuX
        y: openTab.y + openTab.menuY
        joinedCorner: "topLeft"
        sidePadding: 0

        MenuHeading { text: "Content type" }
        Repeater {
            model: root.filterOptions
            RadioMenuItem {
                required property string modelData
                required property int index
                text: modelData
                checked: index === root.activeQuery.filterIndex
                showSeparator: index < root.filterOptions.length - 1
                onClicked: {
                    root.activeQuery.chooseFilter(index)
                    root.closeDropdowns()
                }
            }
        }
        // Tied to the content type, so the menu stays open for that choice too
        SwitchMenuItem {
            text: "Downloaded only"
            checked: root.activeQuery.downloadedOnly
            // Always on offline
            enabled: bookOrbit.online
            onClicked: root.activeQuery.setDownloadedOnly(!root.activeQuery.downloadedOnly)
        }
    }
    PopupMenu {
        id: sortMenu
        visible: false
        z: 3
        x: openTab.sortMenuRight - width
        y: openTab.y + openTab.menuY
        joinedCorner: "topRight"
        sidePadding: 0

        MenuHeading { text: "Sort by" }
        Repeater {
            model: root.activeQuery.sortOptions
            RadioMenuItem {
                required property string modelData
                required property int index
                text: modelData
                checked: index === root.activeQuery.sortIndex
                showSeparator: index < root.activeQuery.sortOptions.length - 1
                onClicked: {
                    root.activeQuery.chooseSort(index)
                    root.closeDropdowns()
                }
            }
        }
    }

    // Header menu, dropping from the menu button. As in the system apps, the page below the
    // header fades and tapping it closes the menu.
    Rectangle {
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        visible: menu.visible
        color: Qt.rgba(1, 1, 1, 0.6)
        MouseArea {
            anchors.fill: parent
            onClicked: menu.visible = false
        }
    }
    PopupMenu {
        id: menu
        visible: false
        anchors.top: header.bottom
        anchors.topMargin: Units.dp(1)
        anchors.right: parent.right
        anchors.rightMargin: Units.dp(9)

        // Needs the server's library
        PopupMenuItem {
            visible: bookOrbit.signedIn && bookOrbit.online
            text: "Find books on this device"
            onClicked: {
                menu.visible = false
                findBooksDialog.search()
            }
        }
        PopupMenuItem {
            visible: bookOrbit.signedIn
            text: "Sign out"
            showSeparator: false
            onClicked: {
                menu.visible = false
                root.closePages()
                bookOrbit.signOut()
            }
        }
    }

    SearchPage {
        id: searchPage
        anchors.fill: parent
        visible: root.searching
        headerHeight: header.height
        onBack: root.searching = false
        onBookClicked: (book) => root.openBook(book)
        onBookHeld: (row) => bookMenu.open(row)
    }

    AuthorInfoPage {
        id: authorInfoPage
        anchors.fill: parent
        visible: root.showingAuthor
        onBack: root.showingAuthor = false
        onBooksClicked: {
            const name = authorInfoPage.author.name
            root.openBooksOf("Author", name, "author", [name])
        }
        // Go straight to the series if there's only one
        onSeriesClicked: {
            const names = authorInfoPage.seriesNames
            if (authorInfoPage.seriesCount === 1 && names.length === 1)
                root.openBooksOf("Series", names[0], "series", [names[0]])
            else
                root.openAuthorSeries(authorInfoPage.author.name)
        }
    }

    // An author's series, opened from their info page: Series rows under a toolbar with a
    // back button, e.g. "SERIES / ADRIAN TCHAIKOVSKY"
    Rectangle {
        id: authorSeriesView
        property string name
        anchors.fill: parent
        visible: root.showingAuthorSeries
        color: "white"

        PageHeader {
            id: authorSeriesHeader
            width: parent.width
            title: "Series / " + authorSeriesView.name
            onBack: {
                root.closeDropdowns()
                root.showingAuthorSeries = false
            }
        }
        GroupsPage {
            id: authorSeriesPage
            anchors.top: authorSeriesHeader.bottom
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            model: bookOrbit.authorSeriesList
            jumpSections: bookOrbit.authorSeriesJumpSections
            loaded: bookOrbit.authorSeriesLoaded
            emptyText: "No series"
            sortText: "By " + authorSeriesQuery.sortOptions[authorSeriesQuery.sortIndex]
            sortReversed: authorSeriesQuery.sortReversed
            descending: authorSeriesQuery.sortDescending
            // Jump rail aligned and spaced like the library's
            jumpRailTop: libraryRail.entriesY - y
            jumpRailPitch: libraryRail.pitch
            sortOpen: sortMenu.visible
            tabOverhang: sortMenu.border.width - 1
            onSortClicked: root.toggleDropdown(sortMenu)
            onSortIconClicked: authorSeriesQuery.reverseSort()
            onGroupClicked: (series) => root.openBooksOf("Series", series.name, "series", [series.name])
        }
    }

    BooksOfPage {
        id: booksOfPage
        // Jump rail aligned and spaced like the library's
        jumpRailTop: libraryRail.entriesY
        jumpRailPitch: libraryRail.pitch
        // Sub-toolbar, using the shared menus
        filterText: root.filterOptions[root.booksOfQuery.filterIndex]
        sortText: "By " + root.booksOfQuery.sortOptions[root.booksOfQuery.sortIndex]
        sortReversed: root.booksOfQuery.sortReversed
        sortDescending: root.booksOfQuery.sortDescending
        seriesOrder: root.booksOfQuery.sortFields[root.booksOfQuery.sortIndex] === "seriesIndex"
        downloadedOnly: root.booksOfQuery.downloadedOnly
        filterOpen: filterMenu.visible
        sortOpen: sortMenu.visible
        tabOverhang: filterMenu.border.width - 1
        onFilterClicked: root.toggleDropdown(filterMenu)
        onSortClicked: root.toggleDropdown(sortMenu)
        onSortIconClicked: root.booksOfQuery.reverseSort()
        anchors.fill: parent
        visible: root.showingBooksOf
        onBack: {
            root.closeDropdowns()
            root.showingBooksOf = false
        }
        onBookClicked: (book) => root.openBook(book)
        onBookHeld: (row) => bookMenu.open(row)
    }

    BookInfoPage {
        id: bookInfoPage
        anchors.fill: parent
        visible: root.showingInfo
        onBack: root.showingInfo = false
        // Close this page, since the books-of page is underneath
        onBooksOfChosen: (field, values, title) => {
            root.showingInfo = false
            root.openBooksOfChosen(field, values, title)
        }
    }

    // Over the pages, opened from any of their book lists
    BookMenu {
        id: bookMenu
        anchors.fill: parent
        onInfoChosen: (book) => {
            bookInfoPage.show(book)
            root.showingInfo = true
        }
        onBooksOfChosen: (field, values, title) => root.openBooksOfChosen(field, values, title)
        onDeleteChosen: (book) => deleteDialog.ask(book)
        onOpenWithChosen: (target, app) => {
            if (target.option === undefined)
                downloads.openWith(target.bookId, target.format, app.app)
            else
                downloadDialog.show(target.option.book, [target.option], app)
        }
    }

    // Confirms deleting a book from the device, as the system library does (screenshot 13).
    // Deletes every version on the device.
    SystemDialog {
        id: deleteDialog
        anchors.fill: parent
        property var book: ({})
        // Set if a file couldn't be deleted
        property bool failed: false

        function ask(newBook) {
            book = newBook
            failed = false
            visible = true
        }

        icon: failed ? "icon_error" : "icon_question"
        buttons: failed ? [["Close", "no"]] : [["No", "no"], ["Yes", "yes"]]
        onPressed: (action) => {
            if (action === "yes") {
                const ids = [book.id]
                for (const variant of book.variants || []) {
                    if (ids.indexOf(variant.bookId) < 0)
                        ids.push(variant.bookId)
                }
                let deleted = true
                for (const id of ids)
                    deleted = downloads.remove(id) && deleted
                if (!deleted) {
                    failed = true
                    return
                }
            }
            visible = false
        }

        Text {
            width: parent.width
            text: deleteDialog.failed
                  ? "Couldn't delete " + (deleteDialog.book.title || "") + " from your device."
                  : "Are you sure you want to delete " + (deleteDialog.book.title || "") + " from your device?"
            font.pointSize: 7.25
            wrapMode: Text.Wrap
        }
    }

    // Above everything, including the search page
    DownloadDialog {
        id: downloadDialog
        anchors.fill: parent
        onOpened: (bookId, format, app) => app ? downloads.openWith(bookId, format, app.app) : downloads.open(bookId)
    }
    FindBooksDialog {
        id: findBooksDialog
        anchors.fill: parent
    }
    // Shown when tapping the title six times turns diagnostics logging on or off
    SystemDialog {
        id: diagnosticsDialog
        anchors.fill: parent
        icon: "icon_information"
        buttons: [["Close", "close"]]
        onPressed: visible = false

        Text {
            width: parent.width
            text: diagnostics.enabled ? "Diagnostics logging is on" : "Diagnostics logging is off"
            font.pointSize: 7.25
            font.bold: true
            wrapMode: Text.Wrap
        }
        Text {
            width: parent.width
            text: diagnostics.enabled
                  ? "Details for bug reports are written to pbookorbit.log on your device's storage."
                  : "Only startup details and errors are written to pbookorbit.log."
            font.pointSize: 7.25
            wrapMode: Text.Wrap
        }
    }
    // On first run, the app adds its tile to the applications screen. It only appears after
    // a restart.
    SystemDialog {
        id: launcherTileDialog
        anchors.fill: parent
        // The device is slow to begin restarting, so the dialog stays up to say so
        property bool restarting: false
        visible: launcherTile.added
        icon: "icon_information"
        buttons: restarting ? [] : [["Restart now", "restart"], ["Later", "close"]]
        onPressed: (action) => {
            if (action === "restart") {
                restarting = true
                restartTimer.start()
            } else
                visible = false
        }

        // Give the screen time to show the restarting message first
        Timer {
            id: restartTimer
            interval: 500
            onTriggered: launcherTile.restartDevice()
        }

        Text {
            width: parent.width
            text: launcherTileDialog.restarting ? "Restarting…" : "Added to your applications"
            font.pointSize: 7.25
            font.bold: true
            wrapMode: Text.Wrap
        }
        Text {
            width: parent.width
            text: launcherTileDialog.restarting
                  ? "Your device is restarting. This can take a minute."
                  : "PBookOrbit now has its own name and icon in your applications."
                    + " Restart your device to see them."
            font.pointSize: 7.25
            wrapMode: Text.Wrap
        }
    }
    // After the first sign-in, offer to find books already on the device, if there are any.
    // Only once; after that it's in the menu.
    Connections {
        target: bookOrbit
        function onSignedInChanged() {
            if (bookOrbit.signedIn && bookFinder.shouldOffer()) {
                bookFinder.markOffered()
                findBooksDialog.offer()
            }
        }
    }
}
