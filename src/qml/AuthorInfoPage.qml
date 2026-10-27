import QtQuick
import "units.js" as Units

// An author's details, opened from their row in the Authors list, laid out like the book
// info page (BookInfoPage): their photo (or initials) where the cover goes, with their
// name, born, died, genres and website beside it, then a row of BOOKS, SERIES and FORMAT,
// whose BOOKS and SERIES open their books and their series, and then what BookOrbit says
// about them. The details beyond the list's row are fetched when the page opens; offline
// they just don't appear. The whole page scrolls when the description is long.
Rectangle {
    id: page
    color: "white"

    // The author's row of the Authors list, as loaded (bookOrbit.authorRow)
    property var author: ({})
    // GET /authors/:id, once it arrives
    property var details: ({})

    signal back()
    signal booksClicked()
    signal seriesClicked()

    function show(authorId) {
        author = bookOrbit.authorRow(authorId)
        details = {}
        content.contentY = 0
        bookOrbit.loadAuthorDetails(authorId)
    }

    Connections {
        target: bookOrbit
        function onAuthorDetails(authorId, newDetails) {
            if (authorId === page.author.id)
                page.details = newDetails
        }
        // Their books' formats and series come in after the list
        function onAuthorsChanged() {
            if (page.visible && page.author.id !== undefined)
                page.author = bookOrbit.authorRow(page.author.id)
        }
    }

    // How many series their books are in, -1 until they have been looked up, and which
    readonly property int seriesCount: author.seriesCount === undefined ? -1 : author.seriesCount
    readonly property var seriesNames: {
        const names = []
        const bookSeries = author.bookSeries || {}
        for (const key in bookSeries) {
            for (const name of bookSeries[key]) {
                if (names.indexOf(name) < 0)
                    names.push(name)
            }
        }
        return names.sort((a, b) => a.localeCompare(b))
    }
    // "12/03/1948", or just the year
    function dateText(date, year) {
        return date ? Qt.formatDate(new Date(date), "dd/MM/yyyy") : (year || "").toString()
    }

    PageHeader {
        id: header
        width: parent.width
        title: "Author info / " + (page.author.name || "")
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
            // As the book info page: 57px margins, the photo 44px below the toolbar
            x: Units.dp(26)
            y: Units.dp(20)
            width: parent.width - 2 * x

            // Photo and the main fields beside it
            Item {
                width: parent.width
                height: Math.max(photoBox.height, fields.height)

                // Where a book's cover goes, as wide, but 3:4, which crops less of most
                // photos than a cover's 2:3
                AuthorPortrait {
                    id: photoBox
                    width: Units.dp(197)
                    height: Units.dp(263)
                    author: page.author
                }

                // 32px after the photo
                Column {
                    id: fields
                    x: photoBox.width + Units.dp(15)
                    width: parent.width - x

                    Repeater {
                        model: [
                            ["Name", page.author.name || ""],
                            ["Born", page.dateText(page.details.birthDate, page.details.birthYear)],
                            ["Died", page.dateText(page.details.deathDate, page.details.deathYear)],
                            ["Genres", (page.details.genres || []).join(", ")],
                            ["Website", page.details.website || ""],
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

            Item { width: 1; height: Units.dp(17) }   // 36px below the photo

            // BOOKS, SERIES, FORMAT, of all their books whatever the list's filter: an ebook
            // and its audiobook count once, as in their book list, once looked up. BOOKS and
            // SERIES go to them, as the book menu's "All books of" items do; SERIES only if
            // they have any.
            InfoCells {
                width: parent.width
                cells: [
                    ["Books", (page.author.bookCount || "").toString(), true],
                    ["Series", page.seriesCount < 0 ? "" : page.seriesCount.toString(), page.seriesCount !== 0],
                    ["Format", (page.author.formats || []).map(f => f.toUpperCase()).join(", ")],
                ]
                onCellClicked: (index) => index === 0 ? page.booksClicked() : page.seriesClicked()
            }
            // A little more room under the cells than between fields, as on the book info page
            Item { width: 1; height: Units.dp(16) }

            InfoField {
                width: parent.width
                label: "Description"
                value: page.details.description || ""
                // HTML from some sources, plain text from others (whose line breaks would
                // be lost as HTML)
                richText: /<[a-z][^>]*>/i.test(value)
                showLine: false
            }
        }
    }

    ListScrollbar {
        list: content
    }
}
