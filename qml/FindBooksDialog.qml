import QtQuick
import "units.js" as Units

// Finds the books already on the device that are in the BookOrbit library (see
// BookFinder), so they show as downloaded. offer() asks first, after the first sign-in;
// search() starts straight away, from the menu.
SystemDialog {
    id: dialog
    // Asking whether to search, before it starts
    property bool offering: false
    property int fileCount: 0
    readonly property string step: offering ? "offer" : bookFinder.state
    readonly property var unsure: bookFinder.unsure

    function offer() {
        fileCount = bookFinder.unrecordedCount()
        offering = true
        visible = true
    }

    function search() {
        offering = false
        visible = true
        bookFinder.start()
    }

    function close() {
        if (bookFinder.state === "searching")
            bookFinder.cancel()
        offering = false
        visible = false
    }

    function books(count) {
        return count === 1 ? "1 book" : count + " books"
    }

    // "Dune.epub as Dune", a line each, the first few
    function unsureLines() {
        const lines = unsure.slice(0, 4).map(u => "• " + u.fileName + " as " + u.title)
        if (unsure.length > 4)
            lines.push("• and " + (unsure.length - 4) + " more")
        return lines.join("\n")
    }

    readonly property string heading: step === "offer" ? "Find your books?"
                                      : step === "searching" ? "Finding your books"
                                      : step === "failed" ? "Couldn't find your books"
                                      : "Your books"

    readonly property string message: {
        if (step === "offer")
            return "There " + (fileCount === 1 ? "is 1 book" : "are " + fileCount + " books")
                   + " on this device already. Find the ones in your BookOrbit library, so they"
                   + " show as downloaded? You can do this later from the menu."
        if (step === "searching")
            return "Looking through your library…  " + bookFinder.percent + "%"
        if (step === "failed")
            return bookFinder.error
        const found = bookFinder.foundCount
        const others = bookFinder.notFoundCount
        let text = found === 0 && others === 0 && unsure.length === 0
                   ? "Every book on this device is already listed."
                   : found === 0 ? "None of the books on this device are in your BookOrbit library."
                   : "Found " + books(found) + " from your BookOrbit library, now listed as downloaded."
        if (unsure.length > 0)
            text += "\n\n" + (unsure.length === 1 ? "1 more looks like" : unsure.length + " more look like")
                    + " a book in your library, going by the file name:\n" + unsureLines()
                    + "\n\nAdd " + (unsure.length === 1 ? "it" : "them") + " too?"
        else if (found > 0 && others > 0)
            text += " " + (others === 1 ? "1 other isn't." : books(others) + " others aren't.")
        return text
    }

    icon: step === "failed" ? "icon_error" : "icon_question"
    percent: step === "searching" ? bookFinder.percent : -1
    buttons: step === "offer" ? [["Find books", "search"], ["Not now", "close"]]
             : step === "searching" ? [["Cancel", "close"]]
             : step === "failed" ? [["Try again", "search"], ["Close", "close"]]
             : unsure.length > 0 ? [["Add " + (unsure.length === 1 ? "it" : "them"), "addUnsure"], ["Skip", "close"]]
             : [["OK", "close"]]

    onPressed: (action) => {
        if (action === "search")
            search()
        else if (action === "addUnsure") {
            bookFinder.addUnsure()
            close()
        } else
            close()
    }

    Text {
        width: parent.width
        text: dialog.heading
        font.pointSize: 8
        font.bold: true
        wrapMode: Text.Wrap
    }
    Text {
        width: parent.width
        text: dialog.message
        font.pointSize: 8
        wrapMode: Text.Wrap
    }
}
