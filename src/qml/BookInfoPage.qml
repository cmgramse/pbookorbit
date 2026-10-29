import QtQuick
import "units.js" as Units
import "night.js" as Night

// A book's details, laid out like the system library's Book info page (screenshot 11):
// cover on the left with title, author, series and file path beside it, then ADDED, SIZE
// and FORMAT. Author and series link to all their books, like the book menu's "All books
// of" items. Below that come the extra details from BookOrbit (published, pages or length,
// language, description), fetched on open and simply missing offline. The page scrolls when
// the description is long, with page buttons like the lists.
//
// A book with an ebook and an audiobook has an AUDIOBOOK | EBOOK switch under the fields
// beside the cover. The boxes and details below describe the chosen version: the one on the
// device at first, else the ebook.
//
// CHOOSE ACTION at the bottom opens a menu in the middle of the page, as on the system's
// page (screenshots 21 and 22), for the chosen version: Open book or Download, Open with,
// and Delete.
//
// The system's page also has a favourite heart, and Add to Collection in its menu.
Rectangle {
    id: page
    color: "white"

    // The book's list entry
    property var book: ({})
    // GET /books/:id by record id, as they load
    property var detailsById: ({})
    // The version described, "ebook" or "audiobook", when the book has both
    property string kind: ""

    readonly property alias pageButtons: pageButtons
    readonly property alias menuOpen: actions.visible

    signal back()
    // Same as BookMenu's: field "author" or "series", the names to match, and the page
    // title
    signal booksOfChosen(string field, var values, string title)
    // The menu's actions, for the chosen version. kind is "" if the book has just one.
    signal openChosen(int bookId, string kind)
    // versions as in DownloadDialog's options
    signal downloadChosen(var book, var versions)
    signal deleteChosen(var book, string kind)
    // As BookMenu's
    signal openWithChosen(var target, var app)

    function show(newBook) {
        book = newBook
        detailsById = {}
        actions.visible = false
        content.contentY = 0
        // The version on the device, if just one is
        let onDevice = []
        for (const version of book.variants || [])
            onDevice = onDevice.concat(downloads.kindsOnDevice(version.bookId))
        kind = onDevice.indexOf("ebook") < 0 && onDevice.indexOf("audiobook") >= 0 ? "audiobook" : "ebook"
        loadDetails()
    }

    function choose(newKind) {
        kind = newKind
        loadDetails()
    }

    // The described record's details, unless already loaded
    function loadDetails() {
        if (record.id !== undefined && detailsById[record.id] === undefined)
            bookOrbit.loadBookDetails(record.id)
    }

    Connections {
        target: bookOrbit
        function onBookDetails(bookId, newDetails) {
            if (page.records.every(r => r.id !== bookId))
                return
            const loaded = Object.assign({}, page.detailsById)
            loaded[bookId] = newDetails
            page.detailsById = loaded
        }
    }

    // "1.9 MB"
    function sizeText(bytes) {
        return bytes >= 1024 * 1024 * 1024 ? (bytes / (1024 * 1024 * 1024)).toFixed(1) + " GB"
             : bytes >= 1024 * 1024 ? (bytes / (1024 * 1024)).toFixed(1) + " MB"
             : Math.max(1, Math.round(bytes / 1024)) + " KB"
    }

    // "13 h 56 min", "45 min", or "" if unknown
    function durationText(seconds) {
        if (!seconds)
            return ""
        const minutes = Math.max(1, Math.round(seconds / 60))
        const hours = Math.floor(minutes / 60)
        return hours > 0 ? hours + " h " + (minutes % 60) + " min" : minutes + " min"
    }

    // Its own record plus any separately stored version (e.g. an audiobook)
    readonly property var records: {
        const records = book.id === undefined ? [] : [book]
        for (const variant of book.variants || []) {
            if (records.every(r => r.id !== variant.bookId))
                records.push(variant.book)
        }
        return records
    }
    // Whether there's an ebook and an audiobook to switch between
    readonly property bool hasKinds: {
        const kinds = (book.variants || []).map(v => v.kind)
        return kinds.indexOf("ebook") >= 0 && kinds.indexOf("audiobook") >= 0
    }
    // The record described: the chosen version's, which may be the book's own
    readonly property var record: {
        const version = hasKinds ? (book.variants || []).find(v => v.kind === kind) : undefined
        return version ? version.book : book
    }
    // GET /books/:id for the record, once loaded
    readonly property var details: detailsById[record.id] || ({})
    // The chosen version, as in DownloadDialog's options, or null if the book has no file
    readonly property var version: {
        const versions = book.variants || []
        return (hasKinds ? versions.find(v => v.kind === kind) : versions[0]) || null
    }
    readonly property bool versionOnDevice: downloads.revision >= 0 && version !== null
                                            && downloads.kindsOnDevice(version.bookId).indexOf(version.kind) >= 0
    // What Open with opens, as in BookMenu: the file on the device, or the version to
    // download first. null if no app takes its format.
    readonly property var openWithTarget: {
        if (version === null || downloads.revision < 0)
            return null
        if (versionOnDevice) {
            for (const format of downloads.formatsOnDevice(version.bookId)) {
                const apps = downloads.kindOf(format) === version.kind ? downloads.appsFor(format) : []
                if (apps.length > 0)
                    return { bookId: version.bookId, format: format, apps: apps }
            }
            return null
        }
        const apps = downloads.appsFor(version.format)
        return apps.length > 0 ? { bookId: version.bookId, format: version.format, apps: apps, option: version } : null
    }
    // One file per format, in the row's order: [{format, sizeBytes}]. Only the chosen
    // version's when there's a choice.
    readonly property var files: {
        const files = []
        for (const record of records) {
            for (const file of record.files || []) {
                const format = (file.format || "").toLowerCase()
                if (format !== "" && (file.role === "primary" || file.role === "content")
                        && (!hasKinds || downloads.kindOf(format) === kind)
                        && files.every(f => f.format !== format))
                    files.push({ format: format, sizeBytes: file.sizeBytes || 0 })
            }
        }
        return files
    }
    // Files on the device, shown as the system shows them: "Internal
    // memory/Books/Dune.epub"
    readonly property var devicePaths: {
        if (downloads.revision < 0)
            return []
        let paths = []
        for (const record of records)
            paths = paths.concat(downloads.pathsOnDevice(record.id))
        // Only the chosen version's, when there's a choice
        if (hasKinds)
            paths = paths.filter(path => downloads.kindOf(path.split(".").pop()) === kind)
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
        // Room at the end to scroll the last line clear of the page buttons
        contentHeight: column.height + Units.dp(26) + pageButtons.height

        // Fast black-and-white updates while scrolling, as in the book lists
        onMovementStarted: {
            const pos = mapToGlobal(0, 0)
            eink.enterA2ForFlick(pos.x, pos.y, width, height)
        }
        onMovementEnded: eink.leaveA2Soon()
        onVisibleChanged: if (!visible) eink.leaveA2()

        Column {
            id: column
            // Margins and cover position as on the system's Book info page
            x: Units.dp(26)
            y: Units.dp(20)
            width: parent.width - 2 * x

            // Cover with the main fields beside it
            Item {
                width: parent.width
                height: Math.max(cover.height, kindSwitch.visible ? kindSwitch.y + kindSwitch.height : fields.height)

                // Scaled to fit. Shows the system's blank book while downloading or if
                // there's none.
                Item {
                    id: cover
                    width: Units.dp(197)
                    height: Units.dp(297)

                    // Bumped when the download arrives so the URL is re-fetched. A binding
                    // rather than setting url, so it follows the next book shown.
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

                Column {
                    id: fields
                    x: cover.width + Units.dp(15)
                    width: parent.width - x

                    // Label and value, plus the field and names for links
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

                // AUDIOBOOK | EBOOK, level with the bottom of the cover, or under the fields
                // if they reach further. The chosen half is black, like the footer's tabs.
                Rectangle {
                    id: kindSwitch
                    visible: page.hasKinds
                    x: fields.x
                    y: Math.max(fields.height + Units.dp(10), cover.height - height)
                    width: fields.width
                    height: Units.dp(30)
                    radius: height / 2
                    border.width: 1
                    border.color: "black"

                    Repeater {
                        model: [["Audiobook", "audiobook"], ["Ebook", "ebook"]]
                        Item {
                            id: half
                            required property var modelData
                            required property int index
                            readonly property bool chosen: page.kind === modelData[1] || halfMouse.pressed
                            x: index * kindSwitch.width / 2
                            width: kindSwitch.width / 2
                            height: kindSwitch.height

                            // A rounded end on the outside, square towards the middle
                            Rectangle {
                                visible: half.chosen
                                anchors.fill: parent
                                radius: kindSwitch.radius
                                color: "black"
                            }
                            Rectangle {
                                visible: half.chosen
                                x: half.index === 0 ? parent.width - width : 0
                                width: kindSwitch.radius
                                height: parent.height
                                color: "black"
                            }
                            Text {
                                anchors.centerIn: parent
                                text: half.modelData[0]
                                color: half.chosen ? "white" : "black"
                                font.pointSize: 8
                                font.capitalization: Font.AllUppercase
                            }
                            MouseArea {
                                id: halfMouse
                                anchors.fill: parent
                                onClicked: page.choose(half.modelData[1])
                            }
                        }
                    }
                    // Between the halves
                    Rectangle {
                        x: parent.width / 2
                        width: 1
                        height: parent.height
                        color: "black"
                    }
                }
            }

            Item { width: 1; height: Units.dp(17) }   // space below the cover

            // ADDED, SIZE, FORMAT
            InfoCells {
                width: parent.width
                cells: [
                    ["Added", page.record.addedAt ? Qt.formatDateTime(new Date(page.record.addedAt), "dd/MM/yy hh:mm") : ""],
                    // Size per format, in the same order
                    ["Size", page.files.map(f => page.sizeText(f.sizeBytes)).join(", ")],
                    ["Format", page.files.map(f => f.format.toUpperCase()).join(", ")],
                ]
            }
            // From the book's details, once loaded
            InfoCells {
                width: parent.width
                showTopLine: false
                cells: [
                    ["Published", page.details.publishedDate
                                  ? Qt.formatDate(new Date(page.details.publishedDate), "dd/MM/yyyy")
                                  : (page.details.publishedYear || "").toString()],
                    // Audiobooks have a length instead of pages
                    (page.hasKinds ? page.kind === "ebook" : page.details.pageCount || !page.details.audioMetadata)
                        ? ["Pages", (page.details.pageCount || "").toString()]
                        : ["Length", page.durationText((page.details.audioMetadata || {}).durationSeconds)],
                    ["Language", page.details.language || ""],
                ]
            }
            // A little more space under the cells than between fields, to separate the
            // sections
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

    // CHOOSE ACTION, between the page buttons: rounded at the top, its bottom edge off the
    // screen
    Rectangle {
        visible: page.version !== null
        anchors.horizontalCenter: parent.horizontalCenter
        y: parent.height - pageButtons.height
        width: Units.dp(384)
        height: pageButtons.height + radius
        radius: Units.dp(6)
        border.width: 1
        border.color: "black"
        color: actionMouse.pressed ? "black" : "white"

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            y: (pageButtons.height - height) / 2
            spacing: Units.dp(8)
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "Choose action"
                color: actionMouse.pressed ? "white" : "black"
                font.pointSize: 8
                font.capitalization: Font.AllUppercase
            }
            Image {
                anchors.verticalCenter: parent.verticalCenter
                source: "image://theme/arrow_up" + (actionMouse.pressed ? "@inverted" : "")
            }
        }
        MouseArea {
            id: actionMouse
            anchors.fill: parent
            onClicked: actions.visible = true
        }
    }

    // The menu, in the middle of the faded page. Tapping outside closes it.
    Item {
        id: actions
        anchors.fill: parent
        visible: false
        // Whether Open with is expanded
        property bool openWithOpen: false
        onVisibleChanged: openWithOpen = false

        Rectangle {
            anchors.fill: parent
            color: Qt.rgba(1, 1, 1, 0.6)
        }
        MouseArea {
            anchors.fill: parent
            onClicked: actions.visible = false
        }

        PopupMenu {
            width: Units.dp(280)
            x: (parent.width - width) / 2
            y: (parent.height - height) / 2

            PopupMenuItem {
                visible: page.versionOnDevice
                icon: "e3_menu_open"
                text: "Open book"
                onClicked: {
                    actions.visible = false
                    page.openChosen(page.version.bookId, page.hasKinds ? page.kind : "")
                }
            }
            PopupMenuItem {
                visible: !page.versionOnDevice
                icon: "download_from_cloud"
                text: "Download"
                onClicked: {
                    actions.visible = false
                    page.downloadChosen(page.version.book, [page.version])
                }
            }
            PopupMenuItem {
                visible: page.openWithTarget !== null
                icon: "e3_menu_open"
                text: "Open with"
                arrow: actions.openWithOpen ? "up" : "down"
                showSeparator: actions.openWithOpen || deleteItem.visible
                onClicked: actions.openWithOpen = !actions.openWithOpen
            }
            Repeater {
                model: actions.openWithOpen && page.openWithTarget ? page.openWithTarget.apps : []
                PopupMenuItem {
                    required property var modelData
                    required property int index
                    subItem: true
                    text: modelData.name
                    showSeparator: index < page.openWithTarget.apps.length - 1 || deleteItem.visible
                    onClicked: {
                        const target = page.openWithTarget
                        actions.visible = false
                        page.openWithChosen(target, modelData)
                    }
                }
            }
            PopupMenuItem {
                id: deleteItem
                visible: page.versionOnDevice
                icon: "trash"
                text: "Delete"
                showSeparator: false
                onClicked: {
                    actions.visible = false
                    page.deleteChosen(page.book, page.hasKinds ? page.kind : "")
                }
            }
        }
    }
}
