import QtQuick
import "units.js" as Units

// Finds books on the device that are in the BookOrbit library (see BookFinder) and marks
// them as downloaded, then says how many. Unsure matches aren't offered yet (they need a
// list to review, see TODO.md), so they count as not matched. offer() asks first, after
// the first sign-in; search() starts immediately, from the menu.
SystemDialog {
    id: dialog
    // Asking whether to search, before starting
    property bool offering: false
    property int fileCount: 0
    readonly property string step: offering ? "offer" : bookFinder.state

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
        const others = bookFinder.notFoundCount + bookFinder.unsure.length
        if (found === 0)
            return others === 0 ? "Every book on this device is already listed."
                                : "None of the books on this device could be matched to your BookOrbit library."
        return "Found " + books(found) + " from your BookOrbit library, now listed as downloaded."
               + (others === 0 ? "" : others === 1 ? " 1 other couldn't be matched."
                                  : " " + books(others) + " others couldn't be matched.")
    }

    icon: step === "failed" ? "icon_error" : "icon_question"
    percent: step === "searching" ? bookFinder.percent : -1
    buttons: step === "offer" ? [["Find books", "search"], ["Not now", "close"]]
             : step === "searching" ? [["Cancel", "close"]]
             : step === "failed" ? [["Try again", "search"], ["Close", "close"]]
             : [["OK", "close"]]

    onPressed: (action) => {
        if (action === "search")
            search()
        else
            close()
    }

    Text {
        width: parent.width
        text: dialog.heading
        font.pointSize: 7.25
        font.bold: true
        wrapMode: Text.Wrap
    }
    Text {
        width: parent.width
        text: dialog.message
        font.pointSize: 7.25
        wrapMode: Text.Wrap
    }
}
