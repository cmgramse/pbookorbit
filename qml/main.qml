import QtQuick
import QtQuick.Window
import "units.js" as Units

Window {
    id: root
    visible: true
    width: screenWidth
    height: windowHeight
    color: "white"

    // Quicker e-ink updates while typing. Applied with callLater because moving focus
    // from one field to the other briefly leaves neither focused.
    readonly property bool typing: (loginPage.visible && loginPage.typing)
                                   || (searchPage.visible && searchPage.typing)
    onTypingChanged: Qt.callLater(() => eink.setDynamicUpdates(root.typing))

    // Search page open over everything else
    property bool searching: false
    // Pages opened from the book menu, a collection or an author, over the search page:
    // an author's info and over that their series, then the books of an author, series or
    // collection, and over that a book's info
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

    // A book on the device opens in its reader (the ebook, if both versions are there);
    // otherwise the dialog offers to download it first, asking which version if it has an
    // ebook and an audiobook
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

    // The title goes back to the top of the current view's list
    function backToTop() {
        menu.visible = false
        closeDropdowns()
        activeList.positionViewAtBeginning()
    }

    // The footer's views: Library, Collections, Series or Authors
    readonly property bool showingCollections: footer.currentIndex === 1
    readonly property bool showingSeries: footer.currentIndex === 2
    readonly property bool showingAuthors: footer.currentIndex === 3
    // The list under the sub-toolbar whose menus are in use
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
        title: "PocketBook × Book<font color=\"" + bookOrbit.accentColor + "\">Orbit</font>"
        menuOpen: menu.visible
        showSearch: bookOrbit.signedIn
        // Its only item is signing out
        showMenu: bookOrbit.signedIn
        onHomeClicked: Qt.quit()
        onTitleClicked: root.backToTop()
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

    // Signed in: the home page. Sub-toolbar with filter and sort, the book list, and a
    // footer for BookOrbit's views, after the Libby app.
    readonly property var filterOptions: ["All titles", "Ebooks", "Audiobooks"]
    // File formats for each filter option (BookOrbit's EBOOK_FORMAT_LIST, AUDIO_FORMAT_LIST)
    readonly property var filterFormats: [
        [],
        ["epub", "kepub", "mobi", "azw3", "azw", "fb2", "pdf", "djvu"],
        ["m4b", "mp3", "m4a", "opus", "ogg", "flac"]
    ]

    // The library's sort and filter. Each sort's usual order: newest first for the dates,
    // A to Z otherwise.
    ListQuery {
        id: libraryQuery
        sortOptions: ["Date added", "Title", "Author", "Recently read"]
        sortFields: ["addedAt", "title", "author", "lastReadAt"]
        sortDescendingByDefault: [true, false, false, true]
        sortIndex: 1   // Title, as bookOrbit starts out
        onApplied: {
            bookOrbit.setBookQuery(sortFields[sortIndex], descending(), root.filterFormats[filterIndex],
                                   downloadedOnly, downloadedOnly ? downloads.downloadedBooks() : [])
            bookList.positionViewAtBeginning()
        }
    }
    // The collections' sorts, from BookOrbit's collections page (less its "Custom order",
    // which isn't clear here): A to Z, and the most books first
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
    // The series' sorts, from BookOrbit's series page: A to Z, and the most books, the
    // latest added and the most read first
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
    // The authors' sorts, from BookOrbit's authors page: A to Z by name or by sort name
    // (surname first), the most books and the latest added first. Filtered as the library
    // is, by their books.
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
    // An author's series: the Series view's sorts, kept from one author to the next
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
    // The books of an author, series or collection, as bookOrbit.showBooksOf() takes them,
    // each kind with its own sort and filter, kept from one to the next as the library's are
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
    function applyBooksOf(query) {
        bookOrbit.showBooksOf(booksOfField, booksOfValues, query.sortFields[query.sortIndex], query.descending(),
                              filterFormats[query.filterIndex], query.downloadedOnly,
                              query.downloadedOnly ? downloads.downloadedBooks() : [])
        booksOfPage.list.positionViewAtBeginning()
    }
    // An author's books: the library's sorts
    ListQuery {
        id: authorBooksQuery
        sortOptions: libraryQuery.sortOptions
        sortFields: libraryQuery.sortFields
        sortDescendingByDefault: libraryQuery.sortDescendingByDefault
        sortIndex: 1
        onApplied: root.applyBooksOf(authorBooksQuery)
    }
    // A series' books: in its order first, then the library's sorts
    ListQuery {
        id: seriesBooksQuery
        sortOptions: ["Series order"].concat(libraryQuery.sortOptions)
        sortFields: ["seriesIndex"].concat(libraryQuery.sortFields)
        sortDescendingByDefault: [false].concat(libraryQuery.sortDescendingByDefault)
        onApplied: root.applyBooksOf(seriesBooksQuery)
    }
    // A collection's books: the library's sorts, but Date added is when each book was added
    // to the collection
    ListQuery {
        id: collectionBooksQuery
        sortOptions: libraryQuery.sortOptions
        sortFields: ["collectionOrder", "title", "author", "lastReadAt"]
        sortDescendingByDefault: libraryQuery.sortDescendingByDefault
        sortIndex: 1
        onApplied: root.applyBooksOf(collectionBooksQuery)
    }
    // Books arriving on the device (or their details being looked up) change the
    // downloaded books
    readonly property int downloadsRevision: downloads.revision
    onDownloadsRevisionChanged: {
        if (libraryQuery.downloadedOnly)
            libraryQuery.applied()
        if (showingBooksOf && booksOfQuery.downloadedOnly)
            booksOfQuery.applied()
        if (authorsQuery.downloadedOnly)
            authorsQuery.applied()
    }

    // The sort and filter that the menus show and change: those of the page in front
    readonly property ListQuery activeQuery: showingBooksOf ? booksOfQuery
                                             : showingAuthorSeries ? authorSeriesQuery
                                             : showingCollections ? collectionsQuery
                                             : showingSeries ? seriesQuery
                                             : showingAuthors ? authorsQuery : libraryQuery
    readonly property bool dropdownOpen: filterMenu.visible || sortMenu.visible

    function closeDropdowns() {
        filterMenu.visible = false
        sortMenu.visible = false
    }

    function toggleDropdown(dropdown) {
        const open = !dropdown.visible
        closeDropdowns()
        if (open) {
            // If the bar is partly scrolled off, bring it fully into view first
            if (activeList.headerItem.mapToItem(root.contentItem, 0, 0).y < activeList.y)
                activeList.positionViewAtBeginning()
            // Line the menu and the copy of the tab up with the sub-toolbar where it has
            // scrolled to. The list can't move while a menu is open: the fading takes taps.
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
        // No bounce at the ends: on e-ink it is only extra redraws
        boundsBehavior: Flickable.StopAtBounds
        model: bookOrbit.books

        // Load the books on screen, and a screenful either side, once scrolling pauses:
        // a fast flick doesn't fetch every page it passes
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

        // Filter and sort bar, and any status line, scroll away with the books. The first
        // row sits right under the bar, as in the system library.
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
                // Busy only until the first books arrive: later pages load as the list
                // scrolls, and a line appearing above the rows would shift them
                visible: bookOrbit.error !== "" || (bookList.count === 0 && (bookOrbit.busy || libraryQuery.downloadedOnly))
                topPadding: Units.dp(8)
                text: bookOrbit.error !== "" ? bookOrbit.error
                      : bookOrbit.busy ? "Working…"
                      : "No downloaded books" + (libraryQuery.filterIndex === 0 ? "" : " of this type")
                font.pointSize: 8
                wrapMode: Text.Wrap
            }
        }

        // Fast black-and-white updates for just the list while it scrolls. A2 takes
        // screen coordinates, hence mapToGlobal (as in the SDK's qml_test example).
        onMovementStarted: {
            const pos = mapToGlobal(0, 0)
            eink.enterA2ForFlick(pos.x, pos.y, width, height)
        }
        // Sent once the momentum has stopped, not when the finger lifts
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

    // For the A-Z sorts. Starts below the sub-toolbar, clear of the sort button, and stays
    // put when the sub-toolbar scrolls away.
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
        // Its jump rail level with the library's, spaced the same
        jumpRailTop: libraryRail.entriesY - y
        jumpRailPitch: libraryRail.pitch
        sortOpen: sortMenu.visible
        tabOverhang: sortMenu.border.width - 1
        onSortClicked: root.toggleDropdown(sortMenu)
        onSortIconClicked: collectionsQuery.reverseSort()
        // Listed afresh each time the view opens
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
        // Its jump rail level with the library's, spaced the same
        jumpRailTop: libraryRail.entriesY - y
        jumpRailPitch: libraryRail.pitch
        sortOpen: sortMenu.visible
        tabOverhang: sortMenu.border.width - 1
        onSortClicked: root.toggleDropdown(sortMenu)
        onSortIconClicked: seriesQuery.reverseSort()
        // Listed the first time the view opens: a large library has many series, and
        // their formats take a request for every few
        onVisibleChanged: if (visible && !bookOrbit.seriesLoaded) bookOrbit.loadSeries()
        onGroupClicked: (series) => root.openBooksOf("Series", series.name, "series", [series.name])
    }

    GroupsPage {
        id: authorsPage
        visible: bookOrbit.signedIn && root.showingAuthors
        model: bookOrbit.authorsList
        jumpSections: bookOrbit.authorJumpSections
        // Under a filter, the authors show once their books have been looked up
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
        // Its jump rail level with the library's, spaced the same
        jumpRailTop: libraryRail.entriesY - y
        jumpRailPitch: libraryRail.pitch
        sortOpen: sortMenu.visible
        tabOverhang: sortMenu.border.width - 1
        onFilterClicked: root.toggleDropdown(filterMenu)
        onSortClicked: root.toggleDropdown(sortMenu)
        onSortIconClicked: authorsQuery.reverseSort()
        // Listed the first time the view opens, as the series are
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
            // Tapping the current view's tab again goes back to the top of its list
            if (index === currentIndex) {
                root.activeList.positionViewAtBeginning()
            } else {
                root.closeDropdowns()
                currentIndex = index
            }
        }
    }

    // While a filter or sort menu is open the whole window fades, as in Libby; a tap
    // anywhere else closes it
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
    // The open tab, drawn over the fading at the sub-toolbar's position in the list
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
        // Goes with the content type, so the menu stays open to choose that too
        SwitchMenuItem {
            text: "Downloaded only"
            checked: root.activeQuery.downloadedOnly
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

    // Header menu: drops down from the menu button. As in the system apps, the page
    // below the header fades while it is open; a tap there closes it.
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
        anchors.rightMargin: Units.dp(9)   // 20px from the screen edge

        PopupMenuItem {
            visible: bookOrbit.signedIn
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
        // Straight to the one series, if that's all there is
        onSeriesClicked: {
            const names = authorInfoPage.seriesNames
            if (authorInfoPage.seriesCount === 1 && names.length === 1)
                root.openBooksOf("Series", names[0], "series", [names[0]])
            else
                root.openAuthorSeries(authorInfoPage.author.name)
        }
    }

    // An author's series, from their info page: the Series view's rows under a toolbar
    // with a back button, e.g. "SERIES / ADRIAN TCHAIKOVSKY"
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
            // Its jump rail level with the library's, spaced the same
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
        // Its jump rail starts level with the library's, spaced the same
        jumpRailTop: libraryRail.entriesY
        jumpRailPitch: libraryRail.pitch
        // Its sub-toolbar, with the shared menus
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
    }

    // Over the pages, from any of their book lists
    BookMenu {
        id: bookMenu
        anchors.fill: parent
        onInfoChosen: (book) => {
            bookInfoPage.show(book)
            root.showingInfo = true
        }
        onBooksOfChosen: (field, values, title) => {
            root.openBooksOf(field === "series" ? "Series" : values.length > 1 ? "Authors" : "Author",
                             title, field, values)
        }
        onDeleteChosen: (book) => deleteDialog.ask(book)
    }

    // Asks before deleting a book from the device, as the system library does (screenshot
    // 13). Deletes every version of it there, ebook and audiobook.
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

    // Over everything, the search page too
    DownloadDialog {
        id: downloadDialog
        anchors.fill: parent
        onOpened: (bookId) => downloads.open(bookId)
    }
    FindBooksDialog {
        id: findBooksDialog
        anchors.fill: parent
    }
    // Signing in for the first time offers to find the books already on the device, if
    // there are any. Only the once: after that it's in the menu.
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
