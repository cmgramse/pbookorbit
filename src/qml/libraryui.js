.pragma library

// The selector's rules, apart from main.qml so they can be tested

// Choosing needs the server, and a choice
function selectorVisible(signedIn, online, count) {
    return signedIn && online && count > 1
}

// Library and search list every download on the device. Authors stay the selected library's.
function downloadedLabel(count, isAuthors) {
    return count > 1 && !isAuthors ? "Downloaded, all libraries" : "Downloaded only"
}

function emptyTitle(filterIndex) {
    return ["No books", "No ebooks", "No audiobooks"][filterIndex]
}

function emptyText(filterIndex) {
    return filterIndex === 0 ? "This library has no books." : "Choose All titles to see every book."
}
