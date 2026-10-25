import QtQuick
import "units.js" as Units

// The menu for a book, after the system library's (screenshot 12). A row held for two
// seconds is framed for a moment (BookRow's focused), then the window fades, all but that
// row, and the menu opens to its right with an arrow pointing at it: level with the row
// where it fits, otherwise moved up to stay on screen. A tap outside the menu closes it.
//
// For now: Book info, All books of author, All books of series (for a book in one), and
// Delete (for a book on the device). The system's menu also has Mark as read, Add to
// collection, Add to Favorites, Select and Open with.
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
            showSeparator: authorItem.visible || seriesItem.visible || deleteItem.visible
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
            showSeparator: seriesItem.visible || deleteItem.visible
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
            showSeparator: deleteItem.visible
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
            showSeparator: false
            onClicked: {
                bookMenu.close()
                bookMenu.deleteChosen(bookMenu.book)
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
