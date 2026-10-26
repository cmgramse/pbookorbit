import QtQuick
import "units.js" as Units

// One book in the list, after the system library on an Era Color (screenshots 9 and 10):
// cover, then title, author, and a line with the reading status and series. A section's
// first row carries a floating box at its left edge: a short label (a letter, a number in
// a series) in a square, or the date written upwards in a narrow box. A black line runs under each row. The book's formats
// show in small capitals at the right of the author line, all of them where its ebook and
// audiobook share the row: grey, or in a solid pill of the user's BookOrbit accent colour
// with a small e-reader mark for a format on the device (where the system shows its cloud icon). A row standing in for
// another (the audiobook record of a book whose ebook row shows both) is hidden: no
// height at all.
// Tapping the row sends clicked(); holding it for two seconds sends held(), for the book
// menu, which sets focused to frame it in a thick black border first, as the search box
// is while it has focus.
Item {
    id: row
    property var book
    // Formats of its versions downloaded by this app and still there, e.g. ["epub"];
    // revision re-checks when a download arrives
    readonly property var formatsOnDevice: {
        if (book.loaded !== true || downloads.revision < 0)
            return []
        let formats = downloads.formatsOnDevice(book.id)
        for (const variant of book.variants || []) {
            if (variant.bookId !== book.id)
                formats = formats.concat(downloads.formatsOnDevice(variant.bookId))
        }
        return formats
    }
    // Framed while it is about to open the book menu
    property bool focused: false
    signal clicked()
    signal held()
    readonly property bool sectionStart: book.sectionStart
    readonly property bool squareSection: book.sectionLabel.length <= 4

    // Measured in device pixels on the Era Color (2.175px per dp)
    readonly property int rowHeight: Units.dp(83)              // 180px
    readonly property int lineLeft: Units.dp(26)               // 57px: after the date box
    readonly property int contentLeft: Units.dp(33)            // 71px: the cover
    readonly property int contentRight: width - contentLeft    // rows end as far from the right
    readonly property int coverWidth: Units.dp(43)             // 94x142px, 2:3
    readonly property int coverHeight: Units.dp(65)

    width: ListView.view ? ListView.view.width : 0
    height: book.hidden === true ? 0 : rowHeight
    visible: book.hidden !== true

    MouseArea {
        anchors.fill: parent
        enabled: row.book.loaded === true
        pressAndHoldInterval: 1500
        onClicked: row.clicked()
        // A row held this long doesn't send clicked() when it is let go
        onPressAndHold: row.held()
    }

    // Cover, shrunk to fit; an outline while it downloads or if there isn't one
    Item {
        id: cover
        x: row.contentLeft
        anchors.verticalCenter: parent.verticalCenter
        width: row.coverWidth
        height: row.coverHeight

        // Bumped when the download arrives, to ask again; a binding rather than setting url,
        // so a row that changes to another book (when the list is sorted) follows it
        property int revision: 0
        property string url: revision >= 0 && row.book.hasCover
                             ? covers.source(row.book.id, row.book.coverVersion, row.coverWidth, row.coverHeight) : ""
        Connections {
            target: covers
            function onReady(bookId) {
                if (bookId === row.book.id)
                    cover.revision++
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
            id: author
            y: Units.dp(49) - baselineOffset
            // Leave room for the formats at the right
            width: parent.width - (formats.width > 0 ? formats.width + Units.dp(12) : 0)
            text: row.book.authors.join(", ")
            font.pointSize: 7.25
            elide: Text.ElideRight
        }
        // The formats, centred on the author line: "EPUB  AZW3  M4B", those on the device
        // in pills
        Row {
            id: formats
            anchors.right: parent.right
            anchors.rightMargin: Units.dp(2)
            anchors.verticalCenter: author.verticalCenter
            spacing: Units.dp(8)

            Repeater {
                model: row.book.formats || []
                Item {
                    id: format
                    required property string modelData
                    readonly property bool onDevice: row.formatsOnDevice.indexOf(modelData) >= 0
                    anchors.verticalCenter: parent.verticalCenter
                    width: onDevice ? pill.width : label.implicitWidth
                    height: pill.height

                    Rectangle {
                        id: pill
                        visible: format.onDevice
                        width: pillContent.width + 2 * Units.dp(7)
                        height: Units.dp(16)                    // 35px
                        radius: height / 2
                        color: bookOrbit.accentColor
                    }
                    Row {
                        id: pillContent
                        x: format.onDevice ? Units.dp(7) : 0
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: Units.dp(3)

                        // A small e-reader: screen outline and a button below it
                        Rectangle {
                            visible: format.onDevice
                            anchors.verticalCenter: parent.verticalCenter
                            width: Units.dp(5)                  // 11x17px
                            height: Units.dp(8)
                            radius: 2
                            color: "transparent"
                            border.width: 2
                            border.color: bookOrbit.accentTextColor
                            Rectangle {
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 3
                                width: 3
                                height: 1
                                color: bookOrbit.accentTextColor
                            }
                        }
                        Text {
                            id: label
                            anchors.verticalCenter: parent.verticalCenter
                            text: format.modelData.toUpperCase()
                            color: format.onDevice ? bookOrbit.accentTextColor : "#606060"
                            font.pointSize: 6
                        }
                    }
                }
            }
        }
        // Status and series in italics, split by an upright grey bar about 32px either side
        Row {
            id: details
            width: parent.width
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

    // Line under the row, 1px black, from after the date box to the end of the row
    Rectangle {
        anchors.bottom: parent.bottom
        x: row.lineLeft
        width: row.contentRight - x
        height: 1
        color: "black"
    }

    // Section letter or number: a floating square at the top left, over the cover's corner
    Rectangle {
        z: 1    // over the focus frame too
        visible: row.sectionStart && row.squareSection && row.book.sectionLabel !== ""
        x: 0
        y: 0
        width: Units.dp(35)                                     // 76px
        height: width
        border.width: 1
        border.color: "black"

        Text {
            anchors.centerIn: parent
            width: parent.width - Units.dp(4)
            horizontalAlignment: Text.AlignHCenter
            text: row.book.sectionLabel
            font.pointSize: 9
            font.bold: true
            // Smaller for a longer number, such as 10.5
            fontSizeMode: Text.HorizontalFit
            minimumPointSize: 6
        }
    }

    // Section date: a narrow box down the left edge, the date written upwards
    Rectangle {
        z: 1
        visible: row.sectionStart && !row.squareSection
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

    // Focus frame round the row, 5px like the search box's: its sides about halfway
    // between the ends of the line under the row and the screen's edges
    Rectangle {
        x: Math.round(row.lineLeft / 2)
        width: Math.round((row.contentRight + row.width) / 2) - x
        height: parent.height
        visible: row.focused
        color: "transparent"
        border.width: Units.dp(2)
        border.color: "black"
    }
}
