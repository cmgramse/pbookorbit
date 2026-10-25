import QtQuick
import "units.js" as Units

// A book's details, laid out like the system library's Book info page (screenshot 11):
// the cover at the left with title, author, series and the file on the device beside it,
// then a row of ADDED, SIZE and FORMAT. Below those come what BookOrbit knows besides
// (published, pages, language, description), fetched when the page opens; offline they
// just don't appear. The whole page scrolls when the description is long.
//
// The system's page also has a favourite heart and a CHOOSE ACTION button.
Rectangle {
    id: page
    color: "white"

    // The book as the list has it
    property var book: ({})
    // GET /books/:id, once it arrives
    property var details: ({})

    signal back()

    function show(newBook) {
        book = newBook
        details = {}
        content.contentY = 0
        bookOrbit.loadBookDetails(newBook.id)
    }

    Connections {
        target: bookOrbit
        function onBookDetails(bookId, newDetails) {
            if (bookId === page.book.id)
                page.details = newDetails
        }
    }

    // "1.9 MB"
    function sizeText(bytes) {
        return bytes >= 1024 * 1024 * 1024 ? (bytes / (1024 * 1024 * 1024)).toFixed(1) + " GB"
             : bytes >= 1024 * 1024 ? (bytes / (1024 * 1024)).toFixed(1) + " MB"
             : Math.max(1, Math.round(bytes / 1024)) + " KB"
    }

    // Its records: its own, and any other version of it (an audiobook kept apart)
    readonly property var records: {
        const records = book.id === undefined ? [] : [book]
        for (const variant of book.variants || []) {
            if (records.every(r => r.id !== variant.bookId))
                records.push(variant.book)
        }
        return records
    }
    // Their book files, one for each format, as the row lists them: [{format, sizeBytes}]
    readonly property var files: {
        const files = []
        for (const record of records) {
            for (const file of record.files || []) {
                const format = (file.format || "").toLowerCase()
                if (format !== "" && (file.role === "primary" || file.role === "content")
                        && files.every(f => f.format !== format))
                    files.push({ format: format, sizeBytes: file.sizeBytes || 0 })
            }
        }
        return files
    }
    // Its files on the device, as the system shows them: "Internal memory/Books/Dune.epub"
    readonly property var devicePaths: {
        if (downloads.revision < 0)
            return []
        let paths = []
        for (const record of records)
            paths = paths.concat(downloads.pathsOnDevice(record.id))
        return paths.map(path => path.replace(/^.*\/mnt\/ext1\//, "Internal memory/"))
    }

    PageHeader {
        id: header
        width: parent.width
        title: "Book info / " + (page.book.title || "")
        onBack: page.back()
    }

    Flickable {
        id: content
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        width: parent.width
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        contentHeight: column.height + Units.dp(26)

        // Fast black-and-white updates while it scrolls, as in the book lists
        onMovementStarted: {
            const pos = mapToGlobal(0, 0)
            eink.enterA2ForFlick(pos.x, pos.y, width, height)
        }
        onMovementEnded: eink.leaveA2Soon()
        onVisibleChanged: if (!visible) eink.leaveA2()

        Column {
            id: column
            // 57px margins, the cover 44px below the toolbar
            x: Units.dp(26)
            y: Units.dp(20)
            width: parent.width - 2 * x

            // Cover and the main fields beside it
            Item {
                width: parent.width
                height: Math.max(cover.height, fields.height)

                // 429x645px, the cover shrunk to fit; an outline while it downloads or if
                // there isn't one
                Item {
                    id: cover
                    width: Units.dp(197)
                    height: Units.dp(297)

                    property string url: page.book.hasCover
                                         ? covers.source(page.book.id, page.book.coverVersion, width, height) : ""
                    Connections {
                        target: covers
                        function onReady(bookId) {
                            if (bookId === page.book.id)
                                cover.url = covers.source(page.book.id, page.book.coverVersion, cover.width, cover.height)
                        }
                    }
                    Rectangle {
                        anchors.fill: parent
                        visible: coverImage.status !== Image.Ready
                        border.width: 1
                        border.color: "#cccccc"
                    }
                    Image {
                        id: coverImage
                        anchors.centerIn: parent
                        width: Math.min(implicitWidth, parent.width)
                        height: Math.min(implicitHeight, parent.height)
                        fillMode: Image.PreserveAspectFit
                        source: cover.url
                        asynchronous: true
                    }
                }

                // 32px after the cover
                Column {
                    id: fields
                    x: cover.width + Units.dp(15)
                    width: parent.width - x

                    Repeater {
                        model: [
                            ["Title", page.book.title || ""],
                            [(page.book.authors || []).length > 1 ? "Authors" : "Author", (page.book.authors || []).join(", ")],
                            ["Series", page.book.seriesText || ""],
                            [page.devicePaths.length > 1 ? "Files" : "File name", page.devicePaths.join("\n")],
                        ]
                        InfoField {
                            required property var modelData
                            width: fields.width
                            label: modelData[0]
                            value: modelData[1]
                        }
                    }
                }
            }

            Item { width: 1; height: Units.dp(17) }   // 36px below the cover

            // ADDED, SIZE, FORMAT
            InfoCells {
                width: parent.width
                cells: [
                    ["Added", page.book.addedAt ? Qt.formatDateTime(new Date(page.book.addedAt), "dd/MM/yy hh:mm") : ""],
                    // Each format's size, in the same order
                    ["Size", page.files.map(f => page.sizeText(f.sizeBytes)).join(", ")],
                    ["Format", page.files.map(f => f.format.toUpperCase()).join(", ")],
                ]
            }
            // From the book's details, once they arrive
            InfoCells {
                width: parent.width
                showTopLine: false
                cells: [
                    ["Published", page.details.publishedDate
                                  ? Qt.formatDate(new Date(page.details.publishedDate), "dd/MM/yyyy")
                                  : (page.details.publishedYear || "").toString()],
                    ["Pages", (page.details.pageCount || "").toString()],
                    ["Language", page.details.language || ""],
                ]
            }
            // A little more room under the cells than between fields, to set the fields
            // below apart from them
            Item { width: 1; height: Units.dp(16) }

            InfoField {
                width: parent.width
                label: "Publisher"
                valueSize: 8
                value: page.details.publisher || ""
            }
            InfoField {
                width: parent.width
                label: "Genres"
                valueSize: 8
                value: (page.details.genres || []).join(", ")
            }
            InfoField {
                width: parent.width
                label: "Description"
                valueSize: 8
                value: page.details.description || ""
                richText: true
                showLine: false
            }
        }
    }

    ListScrollbar {
        list: content
    }

    // One field: a bold label over its value and a grey line under them; nothing when
    // there is no value
    component InfoField: Column {
        id: field
        property string label
        property string value
        property bool richText: false
        property real valueSize: 8
        property bool showLine: true
        visible: value !== ""
        spacing: Units.dp(6)

        Text {
            width: parent.width
            text: field.label
            font.pointSize: 8
            font.bold: true
            wrapMode: Text.Wrap
        }
        Text {
            width: parent.width
            text: field.value
            // Descriptions come as HTML
            textFormat: field.richText ? Text.RichText : Text.PlainText
            font.pointSize: field.valueSize
            wrapMode: Text.Wrap
        }
        // 21px under the value, then 61px to the next label's baseline
        Item {
            width: parent.width
            height: Units.dp(18)
            Rectangle {
                visible: field.showLine
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width
                height: 1
                color: "#7f7f7f"
            }
        }
    }

    // Three cells side by side, a grey capital label over each value, split by grey
    // lines; cells without a value are left blank, and the row goes if all of them are
    component InfoCells: Item {
        id: cellRow
        property var cells: []
        property bool showTopLine: true
        visible: cells.some(cell => cell[1] !== "")
        height: visible ? Units.dp(82) : 0     // 969 to 1148px

        Rectangle {
            visible: cellRow.showTopLine
            width: parent.width
            height: 1
            color: "#7f7f7f"
        }
        Row {
            anchors.fill: parent
            Repeater {
                model: cellRow.cells
                Item {
                    required property var modelData
                    required property int index
                    width: cellRow.width / 3
                    height: cellRow.height

                    // Line before every cell but the first, short of the row's top and bottom
                    Rectangle {
                        visible: index > 0
                        y: Units.dp(13)
                        width: 1
                        height: parent.height - 2 * Units.dp(13)
                        color: "#7f7f7f"
                    }
                    Column {
                        x: Units.dp(11)
                        width: parent.width - 2 * x
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: Units.dp(8)
                        Text {
                            width: parent.width
                            text: modelData[0]
                            color: "#808080"
                            font.pointSize: 8
                            font.capitalization: Font.AllUppercase
                            elide: Text.ElideRight
                        }
                        Text {
                            width: parent.width
                            text: modelData[1]
                            font.pointSize: 8
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: "#7f7f7f"
        }
    }
}
