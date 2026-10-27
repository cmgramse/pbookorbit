import QtQuick
import "units.js" as Units
import "night.js" as Night

// One collection or series row in the Collections or Series list, laid out like BookRow: a
// stack of two covers (the second smaller and behind, with a collection's icon in a white
// disc over the middle), then the name, then the authors by surname (elided) with their
// formats in small capitals at the right, then the book count in italics. A section's first
// row has its letter in a floating square at the left edge, as in BookRow. A black line
// runs under each row. Tapping sends clicked().
//
// An author row (Authors list) shows their photo, or initials if there's none
// (AuthorPortrait), their name, their books' formats, and book and series counts, e.g. "6
// books | 2 series".
Item {
    id: row
    // A row from bookOrbit.collections, seriesList or authorsList
    property var group
    signal clicked()

    readonly property int rowHeight: Units.dp(83)
    readonly property int lineLeft: Units.dp(26)
    readonly property int contentLeft: Units.dp(33)
    readonly property int contentRight: width - contentLeft
    readonly property int coverWidth: Units.dp(43)             // 2:3
    readonly property int coverHeight: Units.dp(65)
    // Front cover and the smaller one behind it, so both fit in one cover's space
    readonly property int frontWidth: Math.round(coverWidth * 0.86)
    readonly property int frontHeight: Math.round(coverHeight * 0.86)
    readonly property int backWidth: Math.round(coverWidth * 0.74)
    readonly property int backHeight: Math.round(coverHeight * 0.74)
    readonly property bool isAuthor: group.kind === "author"
    readonly property var stackBooks: group.covers || []

    width: ListView.view ? ListView.view.width : 0
    height: rowHeight

    MouseArea {
        anchors.fill: parent
        onClicked: row.clicked()
    }

    // A cover scaled to fit width x height, in its corner of the stack. Shows the system's
    // blank book while downloading.
    component StackCover: Item {
        id: stackCover
        property var book
        property int boxWidth
        property int boxHeight
        // Corner: front cover bottom left, back cover top right
        property bool front: true
        // The cover's natural size, once loaded
        readonly property real coverWidth: image.status === Image.Ready ? image.implicitWidth : boxWidth
        readonly property real coverHeight: image.status === Image.Ready ? image.implicitHeight : boxHeight

        x: front ? 0 : row.coverWidth - coverWidth
        y: front ? row.coverHeight - coverHeight : 0
        width: coverWidth
        height: coverHeight

        // Bumped when a download arrives so the URL is re-fetched. A binding rather than
        // setting url, so the row follows a re-sorted list.
        property int revision: 0
        property string url: revision >= 0 && book.id !== undefined
                             ? covers.source(book.id, book.coverVersion, boxWidth, boxHeight) : ""
        Connections {
            target: covers
            function onReady(bookId) {
                if (bookId === stackCover.book.id)
                    stackCover.revision++
            }
        }
        // A white edge separates the front cover from the one behind
        Rectangle {
            visible: stackCover.front
            anchors.fill: parent
            anchors.margins: -2
            color: "white"
        }
        // Opaque, to hide the cover behind
        CoverPlaceholder {
            visible: image.status !== Image.Ready
        }
        Image {
            id: image
            source: Night.image(stackCover.url)
            asynchronous: true
        }
    }

    Item {
        id: stack
        x: row.contentLeft
        anchors.verticalCenter: parent.verticalCenter
        width: row.coverWidth
        height: row.coverHeight

        StackCover {
            visible: row.stackBooks.length > 1
            book: row.stackBooks.length > 1 ? row.stackBooks[1] : ({})
            boxWidth: row.backWidth
            boxHeight: row.backHeight
            front: false
        }
        StackCover {
            visible: row.stackBooks.length > 0
            book: row.stackBooks.length > 0 ? row.stackBooks[0] : ({})
            boxWidth: row.frontWidth
            boxHeight: row.frontHeight
        }
        // No covers (yet): an outline in their place
        Rectangle {
            anchors.fill: parent
            visible: row.stackBooks.length === 0 && !row.isAuthor
            border.width: 1
            border.color: "#cccccc"
        }

        // Author photo or initials, 3:4, as wide as a cover, centred
        AuthorPortrait {
            visible: row.isAuthor
            anchors.centerIn: parent
            width: row.coverWidth
            height: Math.round(row.coverWidth * 4 / 3)
            author: row.isAuthor ? row.group : ({})
        }
    }

    // Name, authors and count on the same baselines as a book's title, author and status
    Item {
        id: texts
        x: stack.x + stack.width + Units.dp(12)
        width: row.contentRight - x - Units.dp(4)
        height: row.height

        Text {
            y: Units.dp(24) - baselineOffset
            width: parent.width
            text: row.group.name || ""
            font.pointSize: 10
            font.bold: true
            elide: Text.ElideRight
        }
        Text {
            id: authors
            y: Units.dp(49) - baselineOffset
            // Leave room for the formats on the right
            width: parent.width - (formats.width > 0 ? formats.width + Units.dp(12) : 0)
            text: (row.group.authors || []).join(", ")
            font.pointSize: 7.25
            elide: Text.ElideRight
        }
        // Book count, and series count for authors, split by an upright grey bar like a
        // book row's status and series
        Row {
            id: counts
            width: parent.width
            spacing: Units.dp(13)
            readonly property int books: row.group.bookCount || 0
            // An author's series, once their books have loaded
            readonly property int series: row.isAuthor && row.group.seriesCount !== undefined ? row.group.seriesCount : 0

            Text {
                y: Units.dp(73) - baselineOffset
                width: Math.min(implicitWidth, counts.width)
                text: counts.books + (counts.books === 1 ? " book" : " books")
                font.pointSize: 7.25
                font.italic: true
                elide: Text.ElideRight
            }
            Text {
                y: Units.dp(73) - baselineOffset
                visible: counts.series > 0
                text: "|"
                color: "#6e6e71"
                font.pointSize: 7.25
            }
            Text {
                y: Units.dp(73) - baselineOffset
                visible: counts.series > 0
                width: Math.max(0, counts.width - x)
                text: counts.series + " series"
                font.pointSize: 7.25
                font.italic: true
                elide: Text.ElideRight
            }
        }
        // Formats, centred on the author line: "EPUB  AZW3  M4B"
        Row {
            id: formats
            anchors.right: parent.right
            anchors.rightMargin: Units.dp(2)
            anchors.verticalCenter: authors.verticalCenter
            spacing: Units.dp(8)

            Repeater {
                model: row.group.formats || []
                Text {
                    required property string modelData
                    text: modelData.toUpperCase()
                    color: "#606060"
                    font.pointSize: 6
                }
            }
        }
    }

    // Section letter: a floating square at top left, over the stack's corner
    Rectangle {
        z: 1
        visible: row.group.sectionStart === true
        x: 0
        y: 0
        width: Units.dp(35)
        height: width
        border.width: 1
        border.color: "black"

        Text {
            anchors.centerIn: parent
            text: row.group.sectionLabel || ""
            font.pointSize: 9
            font.bold: true
        }
    }

    // 1px black line under the row, as under a book row
    Rectangle {
        anchors.bottom: parent.bottom
        x: row.lineLeft
        width: row.contentRight - x
        height: 1
        color: "black"
    }
}
