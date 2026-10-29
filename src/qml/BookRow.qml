import QtQuick
import "units.js" as Units
import "night.js" as Night

// One book in the list, modelled on the Era Color system library (screenshots 9 and 10):
// cover, then title, author, and a line with reading status and series. A section's first
// row has a floating box at its left edge: a short label (letter or series number) in a
// square, or the date written upwards in a narrow box. A black line runs under each row.
//
// Formats show in small capitals at the right of the author line (all of them when an ebook
// and audiobook share the row): grey, or in an accent-coloured pill with a small e-reader
// mark if on the device (where the system shows its cloud icon). A row that another row
// stands in for (an audiobook record whose ebook row shows both) is hidden with zero
// height.
//
// Tapping sends clicked(). Holding for two seconds sends held() for the book menu, which
// first sets focused to draw a thick black frame, like the focused search box.
Item {
    id: row
    property var book
    // Formats of this book's downloads still on the device, e.g. ["epub"]. revision
    // re-checks when a download arrives.
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
    // Framed while about to open the book menu
    property bool focused: false
    signal clicked()
    signal held()
    readonly property bool sectionStart: book.sectionStart
    readonly property bool squareSection: book.sectionLabel.length <= 4

    readonly property int rowHeight: Units.dp(83)
    readonly property int lineLeft: Units.dp(26)               // after the date box
    readonly property int contentLeft: Units.dp(33)            // the cover
    readonly property int contentRight: width - contentLeft    // rows end as far from the right
    readonly property int coverWidth: Units.dp(43)             // 2:3
    readonly property int coverHeight: Units.dp(65)

    width: ListView.view ? ListView.view.width : 0
    height: book.hidden === true ? 0 : rowHeight
    visible: book.hidden !== true

    TapArea {
        anchors.fill: parent
        enabled: row.book.loaded === true
        pressAndHoldInterval: 1500
        onTapped: row.clicked()
        // After a hold this long, releasing doesn't send clicked()
        onPressAndHold: row.held()
    }

    // Cover, scaled to fit. Shows the system's blank book while downloading or if there's
    // none.
    Item {
        id: cover
        x: row.contentLeft
        anchors.verticalCenter: parent.verticalCenter
        width: row.coverWidth
        height: row.coverHeight

        // Bumped when the download arrives so the URL is re-fetched. A binding rather than
        // setting url, so the row follows a re-sorted list.
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
        CoverPlaceholder {
            visible: image.status !== Image.Ready
        }
        Image {
            id: image
            anchors.centerIn: parent
            source: Night.image(cover.url)
            asynchronous: true
        }
    }

    // Title, author, then status and series, each on a fixed baseline so rows with fewer
    // lines align
    Item {
        id: texts
        x: cover.x + cover.width + Units.dp(12)
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
            // Leave room for the formats on the right
            width: parent.width - (formats.width > 0 ? formats.width + Units.dp(12) : 0)
            text: row.book.authors.join(", ")
            font.pointSize: 7.25
            elide: Text.ElideRight
        }
        // Formats, centred on the author line: "EPUB  AZW3  M4B", with downloaded ones in
        // pills
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
                        height: Units.dp(16)
                        radius: height / 2
                        color: Night.color(bookOrbit.accentColor)
                    }
                    Row {
                        id: pillContent
                        x: format.onDevice ? Units.dp(7) : 0
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: Units.dp(3)

                        // A small e-reader: screen outline with a button below
                        Rectangle {
                            visible: format.onDevice
                            anchors.verticalCenter: parent.verticalCenter
                            width: Units.dp(5)
                            height: Units.dp(8)
                            radius: 2
                            color: "transparent"
                            border.width: 2
                            border.color: Night.color(bookOrbit.accentTextColor)
                            Rectangle {
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 3
                                width: 3
                                height: 1
                                color: Night.color(bookOrbit.accentTextColor)
                            }
                        }
                        Text {
                            id: label
                            anchors.verticalCenter: parent.verticalCenter
                            text: format.modelData.toUpperCase()
                            color: format.onDevice ? Night.color(bookOrbit.accentTextColor) : "#606060"
                            font.pointSize: 6
                        }
                    }
                }
            }
        }
        // Status and series in italics, split by an upright grey bar
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

    // 1px black line under the row, from after the date box to the row's end
    Rectangle {
        anchors.bottom: parent.bottom
        x: row.lineLeft
        width: row.contentRight - x
        height: 1
        color: "black"
    }

    // Section letter or number: a floating square at top left, over the cover's corner
    Rectangle {
        z: 1    // above the focus frame too
        visible: row.sectionStart && row.squareSection && row.book.sectionLabel !== ""
        x: 0
        y: 0
        width: Units.dp(35)
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
            // Smaller for longer numbers such as 10.5
            fontSizeMode: Text.HorizontalFit
            minimumPointSize: 6
        }
    }

    // Section date: a narrow box down the left edge, written upwards
    Rectangle {
        z: 1
        visible: row.sectionStart && !row.squareSection
        x: 0
        y: 1
        width: row.lineLeft
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

    // Focus frame, a pixel thinner than the search box's, slightly nearer the screen edges
    // than the line's start
    Rectangle {
        x: Math.round(row.lineLeft * 2 / 5)
        width: row.width - 2 * x
        height: parent.height
        visible: row.focused
        color: "transparent"
        border.width: Units.dp(2) - 1
        border.color: "black"
    }
}
