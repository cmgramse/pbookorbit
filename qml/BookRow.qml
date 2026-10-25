import QtQuick
import "units.js" as Units

// One book in the list, after the system library on an Era Color (screenshots 9 and 10):
// cover, then title, author, and a line with the reading status and series. A section's
// first row carries a floating box at its left edge: the letter in a square, or the date
// written upwards in a narrow box. A black line runs under each row. Books on the device
// get a small e-reader mark at the bottom right (where the system shows its cloud icon).
// Tapping the row sends clicked().
Item {
    id: row
    property var book
    // Downloaded by this app and still there; revision re-checks when a download arrives
    readonly property bool onDevice: book.loaded === true && downloads.revision >= 0
                                     && downloads.isOnDevice(book.id)
    signal clicked()
    readonly property bool sectionStart: book.sectionStart
    readonly property bool letterSection: book.sectionLabel.length === 1

    // Measured in device pixels on the Era Color (2.175px per dp)
    readonly property int rowHeight: Units.dp(83)              // 180px
    readonly property int lineLeft: Units.dp(26)               // 57px: after the date box
    readonly property int contentLeft: Units.dp(33)            // 71px: the cover
    readonly property int contentRight: width - contentLeft    // rows end as far from the right
    readonly property int coverWidth: Units.dp(43)             // 94x142px, 2:3
    readonly property int coverHeight: Units.dp(65)

    width: ListView.view ? ListView.view.width : 0
    height: rowHeight

    MouseArea {
        anchors.fill: parent
        enabled: row.book.loaded === true
        onClicked: row.clicked()
    }

    // Cover, shrunk to fit; an outline while it downloads or if there isn't one
    Item {
        id: cover
        x: row.contentLeft
        anchors.verticalCenter: parent.verticalCenter
        width: row.coverWidth
        height: row.coverHeight

        property string url: row.book.hasCover
                             ? covers.source(row.book.id, row.book.coverVersion, row.coverWidth, row.coverHeight) : ""
        Connections {
            target: covers
            function onReady(bookId) {
                if (bookId === row.book.id)
                    cover.url = covers.source(row.book.id, row.book.coverVersion, row.coverWidth, row.coverHeight)
            }
        }
        Rectangle {
            anchors.fill: parent
            visible: image.status !== Image.Ready
            border.width: 1
            border.color: "#cccccc"
        }
        Image {
            id: image
            anchors.centerIn: parent
            source: cover.url
            asynchronous: true
        }
    }

    // Title, author, then status and series, each on a fixed baseline (52, 107 and 159px
    // below the row's top) so rows with fewer lines line up
    Item {
        id: texts
        x: cover.x + cover.width + Units.dp(12)                 // 26px after the cover
        width: row.contentRight - x - Units.dp(4)
        height: row.height

        Text {
            y: Units.dp(24) - baselineOffset
            width: parent.width
            text: row.book.title
            font.pointSize: 10
            font.bold: true
            elide: Text.ElideRight
        }
        Text {
            y: Units.dp(49) - baselineOffset
            width: parent.width
            text: row.book.authors.join(", ")
            font.pointSize: 7.25
            elide: Text.ElideRight
        }
        // Status and series in italics, split by an upright grey bar about 32px either side
        Row {
            id: details
            // Leave room for the on-device mark at the bottom right
            width: parent.width - (row.onDevice ? deviceMark.width + Units.dp(8) : 0)
            spacing: Units.dp(13)

            Text {
                id: status
                y: Units.dp(73) - baselineOffset
                visible: text !== ""
                width: Math.min(implicitWidth, details.width)
                text: row.book.statusText
                font.pointSize: 7.25
                font.italic: true
                elide: Text.ElideRight
            }
            Text {
                y: Units.dp(73) - baselineOffset
                visible: status.visible && series.visible
                text: "|"
                color: "#6e6e71"
                font.pointSize: 7.25
            }
            Text {
                id: series
                y: Units.dp(73) - baselineOffset
                visible: text !== ""
                width: Math.max(0, details.width - x)
                text: row.book.seriesText
                font.pointSize: 7.25
                font.italic: true
                elide: Text.ElideRight
            }
        }
    }

    // On-device mark: a small e-reader outline, at the bottom right
    Rectangle {
        id: deviceMark
        visible: row.onDevice
        x: row.contentRight - Units.dp(2) - width
        y: row.height - Units.dp(14) - height                  // 30px above the line
        width: Units.dp(11)
        height: Units.dp(17)
        radius: height / 9
        border.width: 2
        border.color: "black"

        // Button below the screen
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: parent.height / 9
            width: parent.width / 3
            height: 2
            color: "black"
        }
    }

    // Line under the row, 1px black, from after the date box to the end of the row
    Rectangle {
        anchors.bottom: parent.bottom
        x: row.lineLeft
        width: row.contentRight - x
        height: 1
        color: "black"
    }

    // Section letter: a floating square at the top left, over the cover's corner
    Rectangle {
        visible: row.sectionStart && row.letterSection
        x: 0
        y: 0
        width: Units.dp(35)                                     // 76px
        height: width
        border.width: 1
        border.color: "black"

        Text {
            anchors.centerIn: parent
            text: row.book.sectionLabel
            font.pointSize: 9
            font.bold: true
        }
    }

    // Section date: a narrow box down the left edge, the date written upwards
    Rectangle {
        visible: row.sectionStart && !row.letterSection && row.book.sectionLabel !== ""
        x: 0
        y: 1
        width: row.lineLeft                                     // 57px
        height: row.height - 4                                  // a 3px gap before the next box
        border.width: 1
        border.color: "black"

        Text {
            anchors.centerIn: parent
            rotation: -90
            text: row.book.sectionLabel
            font.pointSize: 6
        }
    }
}
