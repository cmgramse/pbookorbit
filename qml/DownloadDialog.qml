import QtQuick
import "units.js" as Units

// Offers to download a book that isn't on the device, shows the download, then opens the
// book. A book with an ebook and an audiobook asks which to download.
SystemDialog {
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

    icon: error !== "" || !hasFile ? "icon_error" : "icon_question"
    percent: downloading ? downloads.percent : -1
    onPressed: (action) => press(action)

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
    buttons: downloading ? [["Cancel", "close"]]
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

    Text {
        width: parent.width
        text: dialog.book.title || ""
        font.pointSize: 7.25
        font.bold: true
        wrapMode: Text.Wrap
    }
    Text {
        width: parent.width
        visible: text !== ""
        text: (dialog.book.authors || []).join(", ")
        font.pointSize: 7.25
        wrapMode: Text.Wrap
    }
    Text {
        width: parent.width
        topPadding: Units.dp(6)
        text: dialog.message
        font.pointSize: 7.25
        wrapMode: Text.Wrap
    }
}
