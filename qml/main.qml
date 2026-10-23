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

    // The book list is the only page for now, so home just goes back to its top
    function goHome() {
        menu.visible = false
        closeDropdowns()
        bookList.positionViewAtBeginning()
    }

    AppHeader {
        id: header
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        title: "PocketBook × BookOrbit"
        menuOpen: menu.visible
        showSearch: bookOrbit.signedIn
        onHomeClicked: root.goHome()
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

    function applyBookQuery() {
        bookOrbit.setBookQuery(sortFields[sortIndex], filterFormats[filterIndex])
        bookList.positionViewAtBeginning()
    }
    onFilterIndexChanged: applyBookQuery()
    onSortIndexChanged: applyBookQuery()
    property int filterIndex: 0
    property int sortIndex: 0
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

        // Load the next page while there is still a screenful to go
        onContentYChanged: {
            if (contentY + 2 * height > contentHeight)
                bookOrbit.loadMoreBooks()
        }

        // Filter and sort bar, and any status line, scroll away with the books
        header: Column {
            width: ListView.view.width
            bottomPadding: Units.dp(8)

            SubToolbar {
                width: parent.width
                filterText: root.filterOptions[root.filterIndex]
                sortText: "By " + root.sortOptions[root.sortIndex]
                filterOpen: filterMenu.visible
                sortOpen: sortMenu.visible
                tabOverhang: filterMenu.border.width - 1
                onFilterClicked: root.toggleDropdown(filterMenu)
                onSortClicked: root.toggleDropdown(sortMenu)
            }
            Text {
                x: Units.dp(26)
                width: parent.width - 2 * x
                visible: bookOrbit.error !== "" || bookOrbit.busy
                topPadding: Units.dp(8)
                text: bookOrbit.busy ? "Working…" : bookOrbit.error
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
            required property var modelData
            book: modelData
        }
    }

    // Scrollbar on the far right, as in the system library. Sized for the whole library, not
    // just the pages loaded so far: rows are all the same height, so the full list is the
    // header plus one row height per book.
    // TODO: floating buttons at the bottom left and right that scroll a whole page, as in
    // the system library (see TODO.md)
    Rectangle {
        id: scrollbar
        readonly property real headerHeight: bookList.headerItem ? bookList.headerItem.height : 0
        readonly property real rowHeight: bookList.count > 0
                                          ? (bookList.contentHeight - headerHeight) / bookList.count : 0
        readonly property real fullHeight: headerHeight + rowHeight * bookOrbit.totalBooks
        readonly property real trackTop: bookList.y + Units.dp(7)
        readonly property real trackHeight: bookList.height - 2 * Units.dp(7)
        readonly property real scrolled: Math.max(0, Math.min(1,
            (bookList.contentY - bookList.originY) / Math.max(1, fullHeight - bookList.height)))

        visible: bookList.visible && rowHeight > 0 && fullHeight > bookList.height
        x: parent.width - Units.dp(8)
        width: Units.dp(4)
        height: Math.max(Units.dp(10), trackHeight * bookList.height / Math.max(1, fullHeight))
        y: trackTop + (trackHeight - height) * scrolled
        color: "black"
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
                    root.sortIndex = index
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
            onClicked: {
                menu.visible = false
                root.searching = false
                bookOrbit.signOut()
            }
        }
        PopupMenuItem {
            text: "Exit"
            showSeparator: false
            onClicked: Qt.quit()
        }
    }

    SearchPage {
        id: searchPage
        anchors.fill: parent
        visible: root.searching
        headerHeight: header.height
        // TODO: real recent searches; sample terms until the search logic exists
        recentSearches: ["door", "the faith"]
        onBack: root.searching = false
        // TODO: run the search
        onSearch: (text) => diagnostics.log("search: " + text)
    }
}
