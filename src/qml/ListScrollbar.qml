import QtQuick
import "units.js" as Units

// Scrollbar at the far right of a list, as in the system library: a black bar sized and
// positioned by how much of the list is visible. Place beside the list in the same parent.
// Book lists have a row per book, loaded or not, so it spans the whole list.
Rectangle {
    id: scrollbar
    property Flickable list
    // Space kept clear at the bottom, e.g. for page buttons
    property real bottomInset: 0

    readonly property real trackTop: list.y + Units.dp(7)
    readonly property real trackHeight: list.height - bottomInset - 2 * Units.dp(7)
    readonly property real scrolled: Math.max(0, Math.min(1,
        (list.contentY - list.originY) / Math.max(1, list.contentHeight - list.height)))

    visible: list.visible && list.contentHeight > list.height
    x: parent.width - Units.dp(8)
    width: Units.dp(4)
    height: Math.max(Units.dp(10), trackHeight * list.height / Math.max(1, list.contentHeight))
    y: trackTop + (trackHeight - height) * scrolled
    color: "black"
}
