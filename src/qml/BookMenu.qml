import QtQuick
import "units.js" as Units

// The menu for a book, after the system library's (screenshot 12). A row held for two
// seconds is framed for a moment (BookRow's focused), then the window fades, all but that
// row, and the menu opens to its right with an arrow pointing at it: level with the row
// where it fits, otherwise moved up to stay on screen. A tap outside the menu closes it.
//
// For now: Book info, All books of author, All books of series (for a book in one), Delete
// (for a book on the device) and Open with, which opens in place to list the apps the
// system has for the book's format (screenshot 16). The system's menu also has Mark as
// read, Add to collection, Add to Favorites and Select.
Item {
    id: bookMenu
    // The book whose menu it is, as the book list has it
    property var book: ({})
    // The row held, until the menu opens; null if it goes meanwhile (the list reloaded)
    property BookRow heldRow: null
    // The row and the list it is in, in this item's coordinates, when the menu opened
    property rect rowRect
    property rect listRect

    signal infoChosen(var book)
    // field "author" or "series", values the names to look for, title for the page
    signal booksOfChosen(string field, var values, string title)
    signal deleteChosen(var book)
    // target as openWithTarget has it, app one of its apps
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
        const rowPos = row.mapToItem(bookMenu, 0, 0)
        const listPos = view.mapToItem(bookMenu, 0, 0)
        rowRect = Qt.rect(rowPos.x, rowPos.y, row.width, row.height)
        listRect = Qt.rect(listPos.x, listPos.y, view.width, view.height)
        book = row.book
        openWithOpen = false
        visible = true
    }

    // The book's own record and any other versions of it (an audiobook kept apart)
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

    // What Open with opens, and in what: the book's file of a format the system has apps
    // for, {bookId, format, apps}, plus option (as DownloadDialog takes it) for a file
    // that has to download first; null if there's none. A file on the device comes first.
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
        // The files a download would fetch, as openBook() offers them
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
    // Whether Open with is showing its apps
    property bool openWithOpen: false

    // The faded window
    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(1, 1, 1, 0.6)
        MouseArea {
            anchors.fill: parent
            onClicked: bookMenu.close()
        }
    }

    // The row, unfaded, over the fading where it is in the list; cut off where the list
    // is. Tapping it closes the menu too.
    Item {
        x: bookMenu.listRect.x
        y: bookMenu.listRect.y
        width: bookMenu.listRect.width
        height: bookMenu.listRect.height
        clip: true

        // Only while open: a row needs a book
        Loader {
            x: bookMenu.rowRect.x - parent.x
            y: bookMenu.rowRect.y - parent.y
            active: bookMenu.visible
            sourceComponent: BookRow {
                width: bookMenu.rowRect.width
                book: bookMenu.book
                onClicked: bookMenu.close()
            }
        }
    }

    PopupMenu {
        id: menu
        // 596px wide and 48px from the screen's right edge, its top level with the row's
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

    // The arrow: the theme's pointer laid over the menu's left border, its tip 16px out,
    // level with the middle of the row, but never past the menu's rounded corners
    Image {
        id: pointer
        source: "image://theme/context_menu_pointer"
        x: menu.x + menu.border.width - width + 1
        y: Math.max(menu.y + menu.radius,
                    Math.min(bookMenu.rowRect.y + bookMenu.rowRect.height / 2 - height / 2,
                             menu.y + menu.height - menu.radius - height))
    }
}
