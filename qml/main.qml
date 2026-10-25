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

    // The book list is the only page for now, so the title just goes back to its top
    function backToTop() {
        menu.visible = false
        closeDropdowns()
        bookList.positionViewAtBeginning()
    }

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
    // TODO: give the views other than Library content
    readonly property var filterOptions: ["All titles", "Ebooks", "Audiobooks"]
    // File formats for each filter option (BookOrbit's EBOOK_FORMAT_LIST, AUDIO_FORMAT_LIST)
    readonly property var filterFormats: [
        [],
        ["epub", "kepub", "mobi", "azw3", "azw", "fb2", "pdf", "djvu"],
        ["m4b", "mp3", "m4a", "opus", "ogg", "flac"]
    ]
    readonly property var sortOptions: ["Date added", "Title", "Author", "Recently read"]
    // BookOrbit sort field for each sort option
    readonly property var sortFields: ["addedAt", "title", "author", "lastReadAt"]

    // Each sort's usual order: newest first for the dates, A to Z otherwise
    readonly property var sortDescendingByDefault: [true, false, false, true]

    // The direction is worked out here rather than read from sortDescending: when the
    // sort and sortReversed change together, that binding may not have caught up yet
    function applyBookQuery() {
        const descending = sortDescendingByDefault[sortIndex] !== sortReversed
        bookOrbit.setBookQuery(sortFields[sortIndex], descending, filterFormats[filterIndex],
                               downloadedOnly, downloadedOnly ? downloads.downloadedBooks() : [])
        bookList.positionViewAtBeginning()
    }
    // Choosing another sort starts it the usual way. The list reloads once, with both
    // settled.
    function chooseSort(index) {
        if (index === sortIndex)
            return
        sortReversed = false
        sortIndex = index
        applyBookQuery()
    }
    function reverseSort() {
        sortReversed = !sortReversed
        applyBookQuery()
    }
    onFilterIndexChanged: applyBookQuery()
    property int filterIndex: 0
    // Only the books on the device, along with the content type
    property bool downloadedOnly: false
    onDownloadedOnlyChanged: applyBookQuery()
    // Books arriving on the device (or their details being looked up) change that list
    readonly property int downloadsRevision: downloads.revision
    onDownloadsRevisionChanged: if (downloadedOnly) applyBookQuery()
    property int sortIndex: 1   // Title, as bookOrbit starts out
    property bool sortReversed: false
    readonly property bool sortDescending: sortDescendingByDefault[sortIndex] !== sortReversed
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
            if (bookList.headerItem.mapToItem(root.contentItem, 0, 0).y < bookList.y)
                bookList.positionViewAtBeginning()
            // Line the menu and the copy of the tab up with the sub-toolbar where it has
            // scrolled to. The list can't move while a menu is open: the fading takes taps.
            openTab.y = bookList.headerItem.mapToItem(root.contentItem, 0, 0).y
            dropdown.visible = true
        }
    }

    ListView {
        id: bookList
        visible: bookOrbit.signedIn
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
                filterText: root.filterOptions[root.filterIndex]
                sortText: "By " + root.sortOptions[root.sortIndex]
                sortReversed: root.sortReversed
                filterOpen: filterMenu.visible
                sortOpen: sortMenu.visible
                tabOverhang: filterMenu.border.width - 1
                onFilterClicked: root.toggleDropdown(filterMenu)
                onSortClicked: root.toggleDropdown(sortMenu)
                onSortIconClicked: root.reverseSort()
            }
            Text {
                id: status
                x: Units.dp(26)
                width: parent.width - 2 * x
                // Busy only until the first books arrive: later pages load as the list
                // scrolls, and a line appearing above the rows would shift them
                visible: bookOrbit.error !== "" || (bookList.count === 0 && (bookOrbit.busy || root.downloadedOnly))
                topPadding: Units.dp(8)
                text: bookOrbit.error !== "" ? bookOrbit.error
                      : bookOrbit.busy ? "Working…"
                      : "No downloaded books" + (root.filterIndex === 0 ? "" : " of this type")
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
        onMovementEnded: eink.leaveA2()
        onVisibleChanged: if (!visible) eink.leaveA2()
        delegate: BookRow {
            required book
            onClicked: root.openBook(book)
        }
    }

    // TODO: floating buttons at the bottom left and right that scroll a whole page, as in
    // the system library (see TODO.md)
    ListScrollbar {
        list: bookList
    }

    // Jump rail, for the A-Z sorts: the whole alphabet down the right, between the rows and
    // the scrollbar. Letters with books are bold; tapping one goes to its section. The
    // system library has nothing like it.
    Item {
        id: jumpRail
        readonly property var alphabet: ["#", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L",
                                         "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z"]
        // In the list's order: Z to A, then #, when the sort is turned round
        readonly property var letters: root.sortDescending ? alphabet.slice().reverse() : alphabet
        // Letter -> the row its section starts at
        readonly property var starts: {
            const starts = {}
            for (const section of bookOrbit.jumpSections)
                starts[section.label] = section.index
            return starts
        }
        // 44px a letter, closer if the list is too short for that
        readonly property int pitch: Math.min(Units.dp(20), Math.floor(height / letters.length))

        visible: bookList.visible && bookOrbit.jumpSections.length > 0
        x: parent.width - Units.dp(33)                 // the 71px right of the rows
        width: Units.dp(33) - Units.dp(10)             // clear of the scrollbar
        // Starts below the sub-toolbar, clear of the sort button, and stays put when the
        // sub-toolbar scrolls away
        readonly property real subToolbarHeight: bookList.headerItem ? bookList.headerItem.height : 0
        y: bookList.y + subToolbarHeight + Units.dp(7)
        height: bookList.height - subToolbarHeight - 2 * Units.dp(7)

        Column {
            id: railLetters
            anchors.verticalCenter: parent.verticalCenter
            Repeater {
                model: jumpRail.letters
                Text {
                    required property string modelData
                    readonly property bool hasBooks: jumpRail.starts[modelData] !== undefined
                    width: jumpRail.width
                    height: jumpRail.pitch
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    text: modelData
                    font.pointSize: 7
                    font.bold: hasBooks
                    color: hasBooks ? "black" : "#b4b4b4"
                }
            }
        }
        MouseArea {
            anchors.fill: parent
            onClicked: (mouse) => {
                const letter = jumpRail.letters[Math.floor((mouse.y - railLetters.y) / jumpRail.pitch)]
                const index = jumpRail.starts[letter]
                if (index === undefined)
                    return
                // The first section shows the sub-toolbar too
                if (index === 0)
                    bookList.positionViewAtBeginning()
                else
                    bookList.positionViewAtIndex(index, ListView.Beginning)
                bookList.loadVisibleBooks()
            }
        }
    }

    TabFooter {
        id: footer
        visible: bookOrbit.signedIn
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        // BookOrbit's views; only Library has content so far
        tabs: [
            { label: "Library", icon: "overdrive_library" },
            { label: "Collections", icon: "e3_filter_bookshelf" },
            { label: "Series", icon: "e3_filter_seria_bold" },
            { label: "Authors", icon: "e3_filter_author" }
        ]
        onTabClicked: (index) => {
            // Tapping the current view's tab again goes back to the top of its list
            if (index === currentIndex)
                bookList.positionViewAtBeginning()
            else
                currentIndex = index
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
        filterText: root.filterOptions[root.filterIndex]
        sortText: "By " + root.sortOptions[root.sortIndex]
        sortReversed: root.sortReversed
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
                checked: index === root.filterIndex
                showSeparator: index < root.filterOptions.length - 1
                onClicked: {
                    root.filterIndex = index
                    root.closeDropdowns()
                }
            }
        }
        // Goes with the content type, so the menu stays open to choose that too
        SwitchMenuItem {
            text: "Downloaded only"
            checked: root.downloadedOnly
            onClicked: root.downloadedOnly = !root.downloadedOnly
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
            model: root.sortOptions
            RadioMenuItem {
                required property string modelData
                required property int index
                text: modelData
                checked: index === root.sortIndex
                showSeparator: index < root.sortOptions.length - 1
                onClicked: {
                    root.chooseSort(index)
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
            text: "Sign out"
            showSeparator: false
            onClicked: {
                menu.visible = false
                root.searching = false
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
    }

    // Over everything, the search page too
    DownloadDialog {
        id: downloadDialog
        anchors.fill: parent
        onOpened: (bookId) => downloads.open(bookId)
    }
}
