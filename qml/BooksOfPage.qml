import QtQuick
import "units.js" as Units

// All the books of an author or series, from the book menu: the library's rows under a
// toolbar with a back button, e.g. "SERIES / A SONG OF ICE AND FIRE". An author's books
// go in A-Z sections, as in the library; a series' in its order, each under its number in
// the series. Either has a jump rail when there are more than fit on the screen, and for a
// series at least 10 books.
// Loads as it scrolls, like the library list. bookOrbit.showBooksOf() fills it.
Rectangle {
    id: page
    color: "white"

    // "Author" or "Series", and the name
    property string kind
    property string name
    // Where the jump rail's first entry goes, in the page's coordinates
    property real jumpRailTop: NaN
    // And the spacing of its entries
    property real jumpRailPitch: NaN

    signal back()
    signal bookClicked(var book)
    signal bookHeld(BookRow row)

    PageHeader {
        id: header
        width: parent.width
        title: page.kind + " / " + page.name
        onBack: page.back()
    }

    ListView {
        id: books
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        width: parent.width
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        model: bookOrbit.booksOf

        function loadVisibleBooks() {
            if (count === 0)
                return
            let first = indexAt(width / 2, contentY)
            let last = indexAt(width / 2, contentY + height - 1)
            if (first < 0)
                first = 0
            if (last < 0)
                last = count - 1
            const screenful = last - first + 1
            bookOrbit.loadBooksOf(first - screenful, last + screenful)
        }
        Timer {
            id: loadTimer
            interval: 150
            onTriggered: books.loadVisibleBooks()
        }
        onContentYChanged: loadTimer.restart()
        onCountChanged: loadTimer.restart()

        // Another author or series starts at the top
        Connections {
            target: bookOrbit
            function onBooksOfChanged() {
                if (bookOrbit.booksOfTotal === -1)
                    books.positionViewAtBeginning()
            }
        }

        // Fast black-and-white updates while it scrolls, as in the library list
        onMovementStarted: {
            const pos = mapToGlobal(0, 0)
            eink.enterA2ForFlick(pos.x, pos.y, width, height)
        }
        onMovementEnded: eink.leaveA2Soon()
        onVisibleChanged: if (!visible) eink.leaveA2()

        delegate: BookRow {
            id: bookRow
            required book
            onClicked: page.bookClicked(book)
            onHeld: page.bookHeld(bookRow)
        }
    }

    ListScrollbar {
        list: books
        bottomInset: booksPages.height
    }
    JumpRail {
        list: books
        sections: bookOrbit.booksOfJumpSections
        alignTop: page.jumpRailTop
        alignPitch: page.jumpRailPitch
        alphabetical: page.kind !== "Series"
        // A series of fewer books is quick enough to scroll
        minSections: page.kind === "Series" ? 16 : 1
        bottomInset: booksPages.height
    }
    PageButtons {
        id: booksPages
        list: books
        anchors.bottom: books.bottom
    }

    // While the first books load, or if that failed
    Text {
        visible: books.count === 0
        anchors.top: header.bottom
        anchors.topMargin: Units.dp(26)
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width - 2 * Units.dp(26)
        horizontalAlignment: Text.AlignHCenter
        text: bookOrbit.booksOfTotal === 0 ? "No books found"
              : bookOrbit.error !== "" ? bookOrbit.error
              : "Loading…"
        color: "#737373"
        font.pointSize: 8
        wrapMode: Text.Wrap
    }
}
