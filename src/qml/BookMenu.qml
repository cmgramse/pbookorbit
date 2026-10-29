import QtQuick
import "units.js" as Units

// A book's menu, modelled on the system library's (screenshot 12). Holding a row for two
// seconds frames it briefly (BookRow's focused), then everything but that row fades and the
// menu opens to its right with an arrow pointing at it, level with the row if it fits or
// moved up to stay on screen. Tapping outside closes it. The list can't scroll meanwhile,
// and the fade leaves a gap that follows the row, so the row itself stays unfaded.
//
// Items: Book info, All books of author, All books of series (if in one), Delete (if on the
// device) and Open with, which expands in place to list the system's apps for the format
// (screenshot 16). The system menu also has Mark as read, Add to collection, Add to
// Favorites and Select.
Item {
    id: bookMenu
    // The book's list entry
    property var book: ({})
    // The held row, until the menu opens; null if it went away meanwhile (the list
    // reloaded)
    property BookRow heldRow: null
    // The row and its list while the menu is open. The row goes null if the list drops it.
    property BookRow shownRow: null
    property ListView shownList: null
    // The row and its list in this item's coordinates
    property rect rowRect
    property rect listRect

    signal infoChosen(var book)
    // field is "author" or "series", values the names to match, title the page title
    signal booksOfChosen(string field, var values, string title)
    signal deleteChosen(var book)
    // target as in openWithTarget, app one of its apps
    signal openWithChosen(var target, var app)

    visible: false

    // Frames the row, then opens the menu beside it
    function open(row) {
        close()
        heldRow = row
        heldRow.focused = true
        focusTimer.restart()
    }

    function close() {
        focusTimer.stop()
        if (heldRow)
            heldRow.focused = false
        heldRow = null
        if (shownList)
            shownList.interactive = true
        shownRow = null
        shownList = null
        visible = false
    }

    // Long enough for the frame to show on e-ink before the menu replaces it
    Timer {
        id: focusTimer
        interval: 400
        onTriggered: bookMenu.show()
    }

    function show() {
        const row = heldRow
        if (!row)
            return
        row.focused = false
        heldRow = null
        const view = row.ListView.view
        if (!view)
            return
        // Also ends a drag the held finger may have started
        view.interactive = false
        shownRow = row
        shownList = view
        placeRow()
        book = row.book
        openWithOpen = false
        visible = true
    }

    // Rows above can still change height (e.g. a companion lookup hiding one)
    function placeRow() {
        if (!shownRow || !shownList)
            return
        const rowPos = shownRow.mapToItem(bookMenu, 0, 0)
        const listPos = shownList.mapToItem(bookMenu, 0, 0)
        rowRect = Qt.rect(rowPos.x, rowPos.y, shownRow.width, shownRow.height)
        listRect = Qt.rect(listPos.x, listPos.y, shownList.width, shownList.height)
    }
    Connections {
        target: bookMenu.shownRow
        function onYChanged() { bookMenu.placeRow() }
        function onHeightChanged() { bookMenu.placeRow() }
    }
    Connections {
        target: bookMenu.shownList
        function onContentYChanged() { bookMenu.placeRow() }
    }

    // The book's own record plus any separately stored version (e.g. an audiobook)
    readonly property var bookIds: {
        const ids = book.id === undefined ? [] : [book.id]
        for (const variant of book.variants || []) {
            if (ids.indexOf(variant.bookId) < 0)
                ids.push(variant.bookId)
        }
        return ids
    }
    readonly property bool onDevice: downloads.revision >= 0 && bookIds.some(id => downloads.isOnDevice(id))
    readonly property var authors: book.authors || []
    readonly property string series: book.seriesName || ""

    // What Open with opens: {bookId, format, apps} for a file in a format the system has
    // apps for, plus option (as DownloadDialog takes it) if it needs downloading first.
    // Prefers a file already on the device. null if none.
    readonly property var openWithTarget: {
        if (book.id === undefined || downloads.revision < 0)
            return null
        for (const id of bookIds) {
            for (const format of downloads.formatsOnDevice(id)) {
                const apps = downloads.appsFor(format)
                if (apps.length > 0)
                    return { bookId: id, format: format, apps: apps }
            }
        }
        // Files a download would fetch, as openBook() offers them
        let options = book.variants || []
        if (options.length === 0) {
            const file = downloads.fileToDownload(book)
            if (file.format !== undefined)
                options = [{ bookId: book.id, fileId: file.id, format: file.format,
                             sizeBytes: file.sizeBytes, kind: "", book: book }]
        }
        for (const option of options) {
            const apps = downloads.appsFor(option.format)
            if (apps.length > 0)
                return { bookId: option.bookId, format: option.format, apps: apps, option: option }
        }
        return null
    }
    // Whether Open with is expanded
    property bool openWithOpen: false

    // The faded window, around the row where it shows in the list
    readonly property color fade: Qt.rgba(1, 1, 1, 0.6)
    readonly property real gapTop: Math.max(rowRect.y, listRect.y)
    readonly property real gapBottom: Math.max(gapTop, Math.min(rowRect.y + rowRect.height,
                                                                listRect.y + listRect.height))
    Rectangle {
        width: parent.width
        height: bookMenu.gapTop
        color: bookMenu.fade
    }
    Rectangle {
        y: bookMenu.gapBottom
        width: parent.width
        height: parent.height - y
        color: bookMenu.fade
    }
    Rectangle {
        y: bookMenu.gapTop
        width: bookMenu.rowRect.x
        height: bookMenu.gapBottom - y
        color: bookMenu.fade
    }
    Rectangle {
        x: bookMenu.rowRect.x + bookMenu.rowRect.width
        y: bookMenu.gapTop
        width: parent.width - x
        height: bookMenu.gapBottom - y
        color: bookMenu.fade
    }
    // Tapping anywhere but the menu closes it, the row included
    MouseArea {
        anchors.fill: parent
        onClicked: bookMenu.close()
    }

    PopupMenu {
        id: menu
        // Top level with the row
        width: Units.dp(274)
        x: parent.width - Units.dp(22) - width
        y: Math.max(bookMenu.listRect.y,
                    Math.min(bookMenu.rowRect.y, parent.height - Units.dp(11) - height))

        PopupMenuItem {
            icon: "info"
            text: "Book info"
            showSeparator: authorItem.visible || seriesItem.visible || deleteItem.visible || openWithItem.visible
            onClicked: {
                bookMenu.close()
                bookMenu.infoChosen(bookMenu.book)
            }
        }
        PopupMenuItem {
            id: authorItem
            visible: bookMenu.authors.length > 0
            icon: "search"
            text: "All books of author"
            showSeparator: seriesItem.visible || deleteItem.visible || openWithItem.visible
            onClicked: {
                bookMenu.close()
                bookMenu.booksOfChosen("author", bookMenu.authors, bookMenu.authors.join(", "))
            }
        }
        PopupMenuItem {
            id: seriesItem
            visible: bookMenu.series !== ""
            icon: "search"
            text: "All books of series"
            showSeparator: deleteItem.visible || openWithItem.visible
            onClicked: {
                bookMenu.close()
                bookMenu.booksOfChosen("series", [bookMenu.series], bookMenu.series)
            }
        }
        PopupMenuItem {
            id: deleteItem
            visible: bookMenu.onDevice
            icon: "trash"
            text: "Delete"
            showSeparator: openWithItem.visible
            onClicked: {
                bookMenu.close()
                bookMenu.deleteChosen(bookMenu.book)
            }
        }
        PopupMenuItem {
            id: openWithItem
            visible: bookMenu.openWithTarget !== null
            icon: "e3_menu_open"
            text: "Open with"
            arrow: bookMenu.openWithOpen ? "up" : "down"
            showSeparator: bookMenu.openWithOpen
            onClicked: bookMenu.openWithOpen = !bookMenu.openWithOpen
        }
        Repeater {
            model: bookMenu.openWithOpen && bookMenu.openWithTarget ? bookMenu.openWithTarget.apps : []
            PopupMenuItem {
                required property var modelData
                required property int index
                subItem: true
                text: modelData.name
                showSeparator: index < bookMenu.openWithTarget.apps.length - 1
                onClicked: {
                    const target = bookMenu.openWithTarget
                    bookMenu.close()
                    bookMenu.openWithChosen(target, modelData)
                }
            }
        }
    }

    // The arrow: the theme's pointer laid over the menu's left border, level with the row's
    // middle but never past the rounded corners
    Image {
        id: pointer
        source: "image://theme/context_menu_pointer"
        x: menu.x + menu.border.width - width + 1
        y: Math.max(menu.y + menu.radius,
                    Math.min(bookMenu.rowRect.y + bookMenu.rowRect.height / 2 - height / 2,
                             menu.y + menu.height - menu.radius - height))
    }
}
