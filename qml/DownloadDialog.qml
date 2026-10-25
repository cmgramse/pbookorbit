import QtQuick
import "units.js" as Units

// Offers to download a book that isn't on the device, shows the download, then opens the
// book. A book with an ebook and an audiobook asks which to download. Laid out like the
// system's own dialogs (InkView's Dialog(), which can't be used from Qt), measured on an
// Era Color: thick black rounded border, a round theme icon on the left with the message
// beside it, and the buttons side by side along the bottom, split by thin grey lines. The
// window fades behind it.
Item {
    id: dialog
    // The book being offered, as in the book list
    property var book: ({})
    // Its versions to choose from: [{bookId, fileId, format, sizeBytes, kind, book}]
    property var options: []
    // The one downloading, or last tried
    property var chosen: null
    readonly property bool hasFile: options.length > 0
    readonly property bool downloading: chosen !== null && downloads.bookId === chosen.bookId
    // Set when a download fails, until the next try
    property string error: ""

    signal opened(int bookId)

    visible: false

    // variants as the book list gives them; without any, the book's primary file
    function show(newBook, variants) {
        book = newBook
        let list = variants || []
        if (list.length === 0) {
            const file = downloads.fileToDownload(newBook)
            if (file.format !== undefined)
                list = [{ bookId: newBook.id, fileId: file.id, format: file.format,
                          sizeBytes: file.sizeBytes, kind: "", book: newBook }]
        }
        options = list
        chosen = options.length === 1 ? options[0] : null
        error = ""
        visible = true
    }

    function close() {
        if (downloading)
            downloads.cancel()
        visible = false
    }

    // "EPUB, 1.9 MB"
    function fileText(option) {
        const bytes = option.sizeBytes || 0
        const size = bytes >= 1024 * 1024 * 1024 ? (bytes / (1024 * 1024 * 1024)).toFixed(1) + " GB"
                   : bytes >= 1024 * 1024 ? (bytes / (1024 * 1024)).toFixed(1) + " MB"
                   : Math.max(1, Math.round(bytes / 1024)) + " KB"
        return option.format.toUpperCase() + ", " + size
    }

    function folderOf(option) {
        return downloads.fileToDownload(option.book, option.fileId).folder
    }

    readonly property string message: {
        if (error !== "")
            return error
        if (!hasFile)
            return "This book has no file to download."
        if (downloading)
            return "Downloading " + fileText(chosen) + "…  " + downloads.percent + "%"
        if (chosen === null) {
            // "Download the ebook (EPUB, 1.9 MB) or the audiobook (M4B, 1.1 GB) and open it?"
            return "Download the " + options.map(o => o.kind + " (" + fileText(o) + ")").join(" or the ")
                   + " and open it?"
        }
        return "Download " + fileText(chosen) + " to the " + folderOf(chosen) + " folder and open it?"
    }

    // The buttons, left to right: [label, action]; an option's index downloads it
    readonly property var buttons: downloading ? [["Cancel", "close"]]
                                   : !hasFile ? [["Close", "close"]]
                                   : error !== "" ? [["Try again", options.indexOf(chosen)], ["Cancel", "close"]]
                                   : chosen === null ? options.map((o, i) => [o.kind, i]).concat([["Cancel", "close"]])
                                   : [["Download", 0], ["Cancel", "close"]]

    function press(action) {
        if (action === "close") {
            close()
            return
        }
        error = ""
        chosen = options[action]
        downloads.download(chosen.book, chosen.fileId)
    }

    Connections {
        target: downloads
        function onDownloaded(bookId) {
            if (!dialog.visible || dialog.chosen === null || bookId !== dialog.chosen.bookId)
                return
            dialog.visible = false
            dialog.opened(bookId)
        }
        function onChanged() {
            // A download of this book ended without arriving
            if (dialog.visible && downloads.bookId === -1 && downloads.error !== "")
                dialog.error = downloads.error
        }
    }

    // The faded window; a tap outside the panel does nothing, so a download isn't
    // cancelled by accident
    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(1, 1, 1, 0.6)
        MouseArea {
            anchors.fill: parent
        }
    }

    Rectangle {
        id: panel
        // Near the bottom of the screen, as the system's dialogs are (screenshot 13): its
        // bottom edge 131px up
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Units.dp(60)
        width: parent.width - 2 * Units.dp(42)                 // 92px from each side
        height: body.height + buttonRow.height + 2 * border.width
        radius: Units.dp(9)
        border.width: Units.dp(4)                               // 9px
        border.color: "black"
        color: "white"

        // Icon and message
        Item {
            id: body
            x: panel.border.width
            y: panel.border.width
            width: panel.width - 2 * x
            height: Math.max(Units.dp(175), message.height + 2 * Units.dp(25))   // 380px or more

            Image {
                id: icon
                x: Units.dp(25)                                 // 54px
                anchors.verticalCenter: parent.verticalCenter
                source: "image://theme/" + (dialog.error !== "" || !dialog.hasFile ? "icon_error" : "icon_question")
            }

            Column {
                id: message
                x: icon.x + icon.width + Units.dp(52)           // 113px after the icon
                width: parent.width - x - Units.dp(25)
                anchors.verticalCenter: parent.verticalCenter
                spacing: Units.dp(6)

                Text {
                    width: parent.width
                    text: dialog.book.title || ""
                    font.pointSize: 8
                    font.bold: true
                    wrapMode: Text.Wrap
                }
                Text {
                    width: parent.width
                    visible: text !== ""
                    text: (dialog.book.authors || []).join(", ")
                    font.pointSize: 8
                    wrapMode: Text.Wrap
                }
                Text {
                    width: parent.width
                    topPadding: Units.dp(6)
                    text: dialog.message
                    font.pointSize: 8
                    wrapMode: Text.Wrap
                }
                // Progress: a black bar filling a 1px outline
                Rectangle {
                    width: parent.width
                    height: Units.dp(8)
                    visible: dialog.downloading
                    border.width: 1
                    border.color: "black"
                    Rectangle {
                        width: parent.width * downloads.percent / 100
                        height: parent.height
                        color: "black"
                    }
                }
            }
        }

        // Buttons along the bottom: a grey line above them and between them, each
        // inverted while pressed
        Row {
            id: buttonRow
            x: panel.border.width
            y: body.y + body.height
            width: panel.width - 2 * x
            height: Units.dp(54)                                // 118px

            Repeater {
                model: dialog.buttons
                Item {
                    required property var modelData
                    required property int index
                    width: buttonRow.width / dialog.buttons.length
                    height: buttonRow.height

                    Rectangle {
                        anchors.fill: parent
                        anchors.topMargin: 1
                        color: "black"
                        visible: press.pressed
                        // Square on top, rounded into the panel's corners below
                        bottomLeftRadius: index === 0 ? panel.radius - panel.border.width : 0
                        bottomRightRadius: index === dialog.buttons.length - 1 ? panel.radius - panel.border.width : 0
                    }
                    Text {
                        anchors.centerIn: parent
                        text: modelData[0]
                        color: press.pressed ? "white" : "black"
                        font.pointSize: 8
                        font.capitalization: Font.AllUppercase
                    }
                    // Line between this button and the one before
                    Rectangle {
                        visible: index > 0
                        width: 1
                        height: parent.height
                        color: "#7f7f7f"
                    }
                    MouseArea {
                        id: press
                        anchors.fill: parent
                        onClicked: dialog.press(modelData[1])
                    }
                }
            }
        }
        // Line above the buttons
        Rectangle {
            x: buttonRow.x
            y: buttonRow.y
            width: buttonRow.width
            height: 1
            color: "#7f7f7f"
        }
    }
}
