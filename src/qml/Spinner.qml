import QtQuick

// The system loading spinner (theme icons loading_icon_1 to 9), centred in the list's row
// area, from below its header (the sub-toolbar) to its bottom. Place beside the list in the
// same parent. Only turns while visible.
Item {
    id: spinner
    property ListView list
    readonly property real headerHeight: list.headerItem ? list.headerItem.height : 0

    x: list.x
    y: list.y + headerHeight
    width: list.width
    height: list.height - headerHeight

    SpinnerImage {
        anchors.centerIn: parent
    }
}
