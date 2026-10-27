import QtQuick
import "units.js" as Units
import "night.js" as Night

// A book's details, laid out like the system library's Book info page (screenshot 11):
// the cover at the left with title, author, series and the file on the device beside it,
// then a row of ADDED, SIZE and FORMAT. Author and series go to all their books, as the
// book menu's "All books of" items do. Below those come what BookOrbit knows besides
// (published, pages or an audiobook's length, language, description), fetched when the
// page opens; offline they just don't appear. The whole page scrolls when the
// description is long, with page buttons as the lists have.
//
// The system's page also has a favourite heart and a CHOOSE ACTION button.
Rectangle {
    id: page
    color: "white"

    // The book as the list has it
    property var book: ({})
    // GET /books/:id, once it arrives
    property var details: ({})

    readonly property alias pageButtons: pageButtons

    signal back()
    // As BookMenu's: "author" or "series", the names to match and the page's title
    signal booksOfChosen(string field, var values, string title)

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

    // "13 h 56 min", "45 min"; "" without one
    function durationText(seconds) {
        if (!seconds)
            return ""
        const minutes = Math.max(1, Math.round(seconds / 60))
        const hours = Math.floor(minutes / 60)
        return hours > 0 ? hours + " h " + (minutes % 60) + " min" : minutes + " min"
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
        // Room at the end to scroll the last of it clear of the page buttons
        contentHeight: column.height + Units.dp(26) + pageButtons.height

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

                // 429x645px, the cover shrunk to fit; the system's blank book while it
                // downloads or if there isn't one
                Item {
                    id: cover
                    width: Units.dp(197)
                    height: Units.dp(297)

                    // Bumped when the download arrives, to ask again; a binding rather than
                    // setting url, so the page follows the next book it shows
                    property int revision: 0
                    property string url: revision >= 0 && page.book.hasCover
                                         ? covers.source(page.book.id, page.book.coverVersion, width, height) : ""
                    Connections {
                        target: covers
                        function onReady(bookId) {
                            if (bookId === page.book.id)
                                cover.revision++
                        }
                    }
                    CoverPlaceholder {
                        visible: coverImage.status !== Image.Ready
                        icon: "book_card_dummy_cover_land"
                    }
                    Image {
                        id: coverImage
                        anchors.centerIn: parent
                        width: Math.min(implicitWidth, parent.width)
                        height: Math.min(implicitHeight, parent.height)
                        fillMode: Image.PreserveAspectFit
                        source: Night.image(cover.url)
                        asynchronous: true
                    }
                }

                // 32px after the cover
                Column {
                    id: fields
                    x: cover.width + Units.dp(15)
                    width: parent.width - x

                    // Label, value, and for a link the field and names to go to
                    Repeater {
                        model: [
                            ["Title", page.book.title || ""],
                            [(page.book.authors || []).length > 1 ? "Authors" : "Author", (page.book.authors || []).join(", "),
                             "author", page.book.authors || []],
                            ["Series", page.book.seriesText || "",
                             "series", page.book.seriesName ? [page.book.seriesName] : []],
                            [page.devicePaths.length > 1 ? "Files" : "File name", page.devicePaths.join("\n")],
                        ]
                        InfoField {
                            required property var modelData
                            width: fields.width
                            label: modelData[0]
                            value: modelData[1]
                            tappable: modelData.length > 2 && modelData[3].length > 0
                            onClicked: page.booksOfChosen(modelData[2], modelData[3], modelData[3].join(", "))
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
                    // An audiobook has no pages, but a length
                    page.details.pageCount || !page.details.audioMetadata
                        ? ["Pages", (page.details.pageCount || "").toString()]
                        : ["Length", page.durationText(page.details.audioMetadata.durationSeconds)],
                    ["Language", page.details.language || ""],
                ]
            }
            // A little more room under the cells than between fields, to set the fields
            // below apart from them
            Item { width: 1; height: Units.dp(16) }

            InfoField {
                width: parent.width
                label: "Publisher"
                value: page.details.publisher || ""
            }
            InfoField {
                width: parent.width
                label: "Genres"
                value: (page.details.genres || []).join(", ")
            }
            InfoField {
                width: parent.width
                label: "Description"
                value: page.details.description || ""
                richText: true
                showLine: false
            }
        }
    }

    ListScrollbar {
        list: content
        bottomInset: pageButtons.height
    }
    PageButtons {
        id: pageButtons
        list: content
        anchors.bottom: content.bottom
    }
}
