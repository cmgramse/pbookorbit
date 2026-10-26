import QtQuick
import "units.js" as Units

// One group of books, a collection or a series, in the Collections or Series list, laid
// out like a book row (BookRow): where the cover goes, a stack of two of its books' covers,
// the second smaller and behind, with a collection's icon in a white disc over the middle;
// then its name in the title's place,
// and its books' authors, in order of surname and cut short with an ellipsis, with their
// formats in small capitals at the right; then how many books it has, in italics where a
// book's status goes. The first row of a section (a letter, when sorted by name) carries
// the letter in a floating square at its left edge, as a book row does. A black line runs
// under each row. Tapping the row sends clicked().
// An author's row (in the Authors list) has their photo where the cover goes, or their
// initials without one (AuthorPortrait), their name in the title's place, just their books'
// formats on the line below, and how many books they have and how many series those are in,
// "6 books | 2 series".
Item {
    id: row
    // A row of bookOrbit.collections, seriesList or authorsList
    property var group
    signal clicked()

    // As in BookRow, measured in device pixels on the Era Color (2.175px per dp)
    readonly property int rowHeight: Units.dp(83)              // 180px
    readonly property int lineLeft: Units.dp(26)               // 57px
    readonly property int contentLeft: Units.dp(33)            // 71px
    readonly property int contentRight: width - contentLeft
    readonly property int coverWidth: Units.dp(43)             // 94x142px, 2:3
    readonly property int coverHeight: Units.dp(65)
    // The front cover, and the one behind it: smaller, so both fit where one cover goes
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

    // A cover shrunk to fit width x height, set in its corner of the stack; an outline
    // while it downloads
    component StackCover: Item {
        id: stackCover
        property var book
        property int boxWidth
        property int boxHeight
        // Which corner it sits in: the front cover bottom left, the one behind top right
        property bool front: true
        // The cover's own size, once it has loaded
        readonly property real coverWidth: image.status === Image.Ready ? image.implicitWidth : boxWidth
        readonly property real coverHeight: image.status === Image.Ready ? image.implicitHeight : boxHeight

        x: front ? 0 : row.coverWidth - coverWidth
        y: front ? row.coverHeight - coverHeight : 0
        width: coverWidth
        height: coverHeight

        // Bumped when a download arrives, to ask again; a binding rather than setting url,
        // so a row that changes to another group (when the list is sorted) follows it
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
        // A white edge round the front cover parts it from the one behind
        Rectangle {
            visible: stackCover.front
            anchors.fill: parent
            anchors.margins: -2
            color: "white"
        }
        Rectangle {
            anchors.fill: parent
            visible: image.status !== Image.Ready
            color: "white"
            border.width: 1
            border.color: "#cccccc"
        }
        Image {
            id: image
            source: stackCover.url
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
        // No covers (yet): an outline where they go
        Rectangle {
            anchors.fill: parent
            visible: row.stackBooks.length === 0 && !row.isAuthor
            border.width: 1
            border.color: "#cccccc"
        }

        // An author's photo, or their initials, 3:4 and as wide as a cover, centred
        AuthorPortrait {
            visible: row.isAuthor
            anchors.centerIn: parent
            width: row.coverWidth
            height: Math.round(row.coverWidth * 4 / 3)
            author: row.isAuthor ? row.group : ({})
        }

        // A collection's icon, in a white disc with a black edge
        Rectangle {
            visible: (row.group.icon || "") !== ""
            anchors.centerIn: parent
            width: Units.dp(26)                                 // 57px
            height: width
            radius: width / 2
            color: "white"
            border.width: 2
            border.color: "black"

            Image {
                id: icon
                anchors.centerIn: parent
                // As for the covers: bumped to ask again when the download arrives
                property int revision: 0
                property string url: revision >= 0 ? collectionIcons.source(row.group.icon || "") : ""
                Connections {
                    target: collectionIcons
                    function onReady(name) {
                        if (name === row.group.icon)
                            icon.revision++
                    }
                }
                source: url
                sourceSize.width: Units.dp(15)
                sourceSize.height: Units.dp(15)
            }
        }
    }

    // Name, authors and book count, on the same baselines as a book's title, author and
    // status (52, 107 and 159px below the row's top)
    Item {
        id: texts
        x: stack.x + stack.width + Units.dp(12)                 // 26px after the stack
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
            // Leave room for the formats at the right
            width: parent.width - (formats.width > 0 ? formats.width + Units.dp(12) : 0)
            text: (row.group.authors || []).join(", ")
            font.pointSize: 7.25
            elide: Text.ElideRight
        }
        // How many books, and for an author how many series, split as a book row's status
        // and series are, by an upright grey bar
        Row {
            id: counts
            width: parent.width
            spacing: Units.dp(13)
            readonly property int books: row.group.bookCount || 0
            // An author's series, once their books are in
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
        // The formats, centred on the author line: "EPUB  AZW3  M4B"
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

    // Section letter: a floating square at the top left, over the stack's corner
    Rectangle {
        z: 1
        visible: row.group.sectionStart === true
        x: 0
        y: 0
        width: Units.dp(35)                                     // 76px
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

    // Line under the row, 1px black, as under a book row
    Rectangle {
        anchors.bottom: parent.bottom
        x: row.lineLeft
        width: row.contentRight - x
        height: 1
        color: "black"
    }
}
