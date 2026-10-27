import QtQuick
import "units.js" as Units

// Jump rail for sectioned lists, between the rows and the scrollbar. Tapping an entry jumps
// to its section. For A-Z it shows the whole alphabet; for a series, every number from 1 to
// the last, or round steps with dots between if they don't fit ("1 • 5 • 10 • 15"). Entries
// with books are bold, the rest grey. Numbers use the letters' spacing, as many as fit.
// Starts where the alphabet's # would, or at alignTop. The system library has nothing like
// it. Place it beside the list in the same parent; it only shows with at least minSections
// sections and a list longer than one screen.
Item {
    id: jumpRail
    property ListView list
    // The list's sections, e.g. [{label: "A", index: 2}, ...] (see bookOrbit.jumpSections)
    property var sections: []
    // Letters, or series numbers
    property bool alphabetical: true
    // Below this many sections the rail isn't worth showing
    property int minSections: 1
    // Z to A then #, or numbers from the last, when the list is reversed
    property bool descending: false
    // Space kept clear at the top (e.g. a header that scrolls away) and the bottom (e.g.
    // page buttons)
    property real topInset: 0
    property real bottomInset: 0
    // Y of the first entry in parent coordinates, to align with another rail's entriesY. If
    // unset, the alphabet is centred on the rail.
    property real alignTop: NaN
    // Y of the first entry, in parent coordinates
    readonly property real entriesY: y + entriesTop
    // Entry spacing, to match another rail's pitch. If unset, as tight as the alphabet
    // needs, up to fullPitch.
    property real alignPitch: NaN

    readonly property var alphabet: ["#", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L",
                                     "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z"]
    // Spacing per entry; letters close up if the list is too short for the full alphabet
    readonly property int fullPitch: Units.dp(20)
    // Top of the alphabet when centred on the rail; numbers start here too
    readonly property real centredTop:
        Math.max(0, (height - alphabet.length * Math.min(fullPitch, Math.floor(height / alphabet.length))) / 2)
    readonly property real entriesTop: isNaN(alignTop) ? centredTop : Math.max(0, alignTop - y)
    // Height available for entries
    readonly property real room: isNaN(alignTop) ? height : height - entriesTop
    readonly property real pitch: isNaN(alignPitch) ? Math.min(fullPitch, Math.floor(room / alphabet.length))
                                                    : alignPitch
    // Entries in list order, [{label, index}], where index is the row to jump to, or
    // undefined if the section is empty
    readonly property var entries: alphabetical ? letterEntries() : descending ? numbers.slice().reverse() : numbers
    readonly property var numbers: alphabetical ? [] : numberEntries(Math.max(2, Math.floor(room / pitch)))

    function letterEntries() {
        const starts = {}
        for (const section of sections)
            starts[section.label] = section.index
        return (descending ? alphabet.slice().reverse() : alphabet).map(letter => ({label: letter, index: starts[letter]}))
    }

    // At most count entries for series numbers. A book numbered 2.5 counts under 2.
    function numberEntries(count) {
        if (sections.length === 0)
            return []
        const numbers = sections.map(section => Math.floor(parseFloat(section.label)))
        // Non-numeric labels: as many sections as fit, evenly spread
        if (numbers.some(isNaN)) {
            const picked = []
            for (let i = 0; i < Math.min(count, sections.length); i++)
                picked.push(sections[Math.round(i * (sections.length - 1) / Math.max(1, Math.min(count, sections.length) - 1))])
            return picked
        }
        // First book numbered between from and to, if any
        const firstIn = (from, to) => {
            for (let i = 0; i < numbers.length; i++) {
                if (numbers[i] >= from && numbers[i] <= to)
                    return sections[i].index
            }
            return undefined
        }
        // From 1 (or 0 if there's a book 0) to the last
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
        // First and last numbers, with the smallest round step between that fits
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
    // The full gap between the row ends and the scrollbar, so centred letters sit midway
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
                // Smaller for longer numbers such as 150
                fontSizeMode: Text.HorizontalFit
                minimumPointSize: 5
                font.bold: hasBooks
                color: hasBooks ? "black" : "#b4b4b4"
            }
        }
    }
    // Wider than the letters, overlapping the row ends and the scrollbar to the screen
    // edge, so it's easier to hit. Taps that miss open the book behind.
    MouseArea {
        x: -Units.dp(16)
        width: jumpRail.parent.width - jumpRail.x - x
        height: parent.height
        onClicked: (mouse) => {
            const entry = jumpRail.entries[Math.floor((mouse.y - railEntries.y) / jumpRail.pitch)]
            if (!entry || entry.index === undefined)
                return
            // The first section also shows the list's header
            if (entry.index === 0)
                jumpRail.list.positionViewAtBeginning()
            else
                jumpRail.list.positionViewAtIndex(entry.index, ListView.Beginning)
            jumpRail.list.loadVisibleBooks()
        }
    }
}
