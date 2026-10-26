import QtQuick

// The system's loading spinner (theme icons loading_icon_1 to 9), centred where a list's
// rows would be: below its header (the sub-toolbar), down to its bottom. Goes beside the
// list, in the same parent. It turns only while visible.
Item {
    id: spinner
    property ListView list
    readonly property real headerHeight: list.headerItem ? list.headerItem.height : 0
    property int frame: 1

    x: list.x
    y: list.y + headerHeight
    width: list.width
    height: list.height - headerHeight

    Image {
        anchors.centerIn: parent
        source: "image://theme/loading_icon_" + spinner.frame
    }
    Timer {
        interval: 200
        repeat: true
        running: spinner.visible
        onTriggered: spinner.frame = spinner.frame % 9 + 1
    }
}
