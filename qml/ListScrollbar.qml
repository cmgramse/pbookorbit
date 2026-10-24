import QtQuick
import "units.js" as Units

// Scrollbar on the far right of a list, as in the system library: a black bar, sized and
// placed by how much of the list is on screen. Goes beside the list, in the same parent.
// The book lists have a row for every book, loaded or not, so it covers the whole list.
Rectangle {
    id: scrollbar
    property Flickable list

    readonly property real trackTop: list.y + Units.dp(7)
    readonly property real trackHeight: list.height - 2 * Units.dp(7)
    readonly property real scrolled: Math.max(0, Math.min(1,
        (list.contentY - list.originY) / Math.max(1, list.contentHeight - list.height)))

    visible: list.visible && list.contentHeight > list.height
    x: parent.width - Units.dp(8)
    width: Units.dp(4)
    height: Math.max(Units.dp(10), trackHeight * list.height / Math.max(1, list.contentHeight))
    y: trackTop + (trackHeight - height) * scrolled
    color: "black"
}
