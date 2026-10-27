import QtQuick
import "units.js" as Units

// Jump rail, for lists in sections: down the right, between the rows and the scrollbar.
// Tapping an entry goes to its section. For A-Z sections it is the whole alphabet; for a
// series, every whole number from 1 to the last, or if they don't fit, round steps with a
// dot for the numbers between ("1 • 5 • 10 • 15"). Entries with books are bold, the rest
// grey. Numbers are spaced as the letters are, as many as fit on the rail. Starts where
// the alphabet's # would, or at alignTop. The system library has nothing like it. Goes
// beside the list, in the same parent, and only shows while the list has at least
// minSections sections and is longer than a screen.
Item {
    id: jumpRail
    property ListView list
    // The list's sections: [{label: "A", index: 2}, ...], as bookOrbit.jumpSections
    property var sections: []
    // Letters, or numbers in a series
    property bool alphabetical: true
    // Fewer sections than this and the rail isn't worth showing
    property int minSections: 1
    // Z to A, then #, or the numbers from the last, when the list is turned round
    property bool descending: false
    // Space kept clear at the top (e.g. a header that scrolls away) and at the bottom
    // (e.g. page buttons)
    property real topInset: 0
    property real bottomInset: 0
    // Where the first entry goes, in the parent's coordinates, to line it up with another
    // rail's (its entriesY); unset, the alphabet is centred on the rail
    property real alignTop: NaN
    // Where the first entry is, in the parent's coordinates
    readonly property real entriesY: y + entriesTop
    // The spacing of the entries, to match another rail's (its pitch); unset, as close as
    // the alphabet needs to fit, at most fullPitch
    property real alignPitch: NaN

    readonly property var alphabet: ["#", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L",
                                     "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z"]
    // 44px an entry; letters close up if the list is too short for the alphabet
    readonly property int fullPitch: Units.dp(20)
    // Where the alphabet, centred on the rail, starts: numbers start there too
    readonly property real centredTop:
        Math.max(0, (height - alphabet.length * Math.min(fullPitch, Math.floor(height / alphabet.length))) / 2)
    readonly property real entriesTop: isNaN(alignTop) ? centredTop : Math.max(0, alignTop - y)
    // The height the entries may take
    readonly property real room: isNaN(alignTop) ? height : height - entriesTop
    readonly property real pitch: isNaN(alignPitch) ? Math.min(fullPitch, Math.floor(room / alphabet.length))
                                                    : alignPitch
    // The entries, in the list's order: [{label, index}], index being the row to go to, or
    // undefined if there are no books there
    readonly property var entries: alphabetical ? letterEntries() : descending ? numbers.slice().reverse() : numbers
    readonly property var numbers: alphabetical ? [] : numberEntries(Math.max(2, Math.floor(room / pitch)))

    function letterEntries() {
        const starts = {}
        for (const section of sections)
            starts[section.label] = section.index
        return (descending ? alphabet.slice().reverse() : alphabet).map(letter => ({label: letter, index: starts[letter]}))
    }

    // At most count entries for the series numbers. A book numbered 2.5 counts under 2.
    function numberEntries(count) {
        if (sections.length === 0)
            return []
        const numbers = sections.map(section => Math.floor(parseFloat(section.label)))
        // Labels that aren't numbers: as many sections as fit, evenly spread
        if (numbers.some(isNaN)) {
            const picked = []
            for (let i = 0; i < Math.min(count, sections.length); i++)
                picked.push(sections[Math.round(i * (sections.length - 1) / Math.max(1, Math.min(count, sections.length) - 1))])
            return picked
        }
        // The first book numbered from to to, if any
        const firstIn = (from, to) => {
            for (let i = 0; i < numbers.length; i++) {
                if (numbers[i] >= from && numbers[i] <= to)
                    return sections[i].index
            }
            return undefined
        }
        // From 1, or 0 if there is a book 0, to the last
        const first = Math.min(1, Math.min(...numbers))
        const last = Math.max(...numbers)
        const marked = (marks) => {
            const entries = []
            for (let i = 0; i < marks.length; i++) {
                if (i > 0 && marks[i] - marks[i - 1] > 1)
                    entries.push({label: "•", index: firstIn(marks[i - 1] + 1, marks[i] - 1)})
                entries.push({label: String(marks[i]), index: firstIn(marks[i], marks[i])})
            }
            return entries
        }
        const all = []
        for (let n = first; n <= last; n++)
            all.push(n)
        if (all.length <= count)
            return marked(all)
        // The first and last numbers, and the round steps between: the smallest step that fits
        let entries = []
        for (const step of [3, 4, 5, 10, 15, 20, 25, 50, 100, 150, 200, 250, 500, 1000]) {
            const marks = [first]
            for (let n = (Math.floor(first / step) + 1) * step; n < last; n += step)
                marks.push(n)
            marks.push(last)
            entries = marked(marks)
            if (entries.length <= count)
                break
        }
        return entries
    }

    visible: list.visible && sections.length > 0 && sections.length >= minSections
             && list.contentHeight > list.height
    // The whole gap between the ends of the rows (71px from the edge) and the scrollbar
    // (dp(8) from it), so the centred letters sit as far from one as the other
    x: parent.width - Units.dp(33)
    width: Units.dp(33) - Units.dp(8)
    y: list.y + topInset + Units.dp(7)
    height: list.height - topInset - bottomInset - 2 * Units.dp(7)

    Column {
        id: railEntries
        y: jumpRail.entriesTop
        Repeater {
            model: jumpRail.entries
            Text {
                required property var modelData
                readonly property bool hasBooks: modelData.index !== undefined
                width: jumpRail.width
                height: jumpRail.pitch
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                text: modelData.label
                font.pointSize: 7
                // Smaller for a longer number, such as 150
                fontSizeMode: Text.HorizontalFit
                minimumPointSize: 5
                font.bold: hasBooks
                color: hasBooks ? "black" : "#b4b4b4"
            }
        }
    }
    // Wider than the letters, reaching a little over the ends of the rows and on over
    // the scrollbar to the edge of the screen, so the rail is easier to hit; a tap that
    // misses it opens the book behind
    MouseArea {
        x: -Units.dp(16)                           // 35px
        width: jumpRail.parent.width - jumpRail.x - x
        height: parent.height
        onClicked: (mouse) => {
            const entry = jumpRail.entries[Math.floor((mouse.y - railEntries.y) / jumpRail.pitch)]
            if (!entry || entry.index === undefined)
                return
            // The first section shows the list's header too
            if (entry.index === 0)
                jumpRail.list.positionViewAtBeginning()
            else
                jumpRail.list.positionViewAtIndex(entry.index, ListView.Beginning)
            jumpRail.list.loadVisibleBooks()
        }
    }
}
