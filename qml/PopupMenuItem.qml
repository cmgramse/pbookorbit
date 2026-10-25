import QtQuick
import "units.js" as Units

// One row of a PopupMenu: optional theme icon, then the label. While pressed the row
// gets a black rounded fill and the icon and label turn white. A grey line separates it
// from the row below. Measured from the library and Sudoku apps on an Era Color.
Item {
    id: item
    property alias text: label.text
    property string icon            // optional theme resource name, e.g. "logout"
    property bool showSeparator: true
    signal clicked()

    width: parent ? parent.width : 0
    height: Units.dp(50)   // 109px
    // Content width, so the menu can grow to fit its widest row
    implicitWidth: label.anchors.leftMargin + label.implicitWidth + label.anchors.rightMargin

    Rectangle {
        anchors.fill: parent
        radius: Units.dp(4)
        color: "black"
        visible: mouse.pressed
    }
    Image {
        // Icon centred 58px from the row's left edge, as in the library app's menu
        x: Units.dp(26) - width / 2
        anchors.verticalCenter: parent.verticalCenter
        visible: item.icon !== ""
        source: item.icon === "" ? "" : "image://theme/" + item.icon + (mouse.pressed ? "@inverted" : "")
    }
    Text {
        id: label
        anchors.left: parent.left
        // Label at 112px with an icon, 39px without
        anchors.leftMargin: item.icon !== "" ? Units.dp(52) : Units.dp(18)
        anchors.right: parent.right
        anchors.rightMargin: Units.dp(10)   // 21px
        anchors.verticalCenter: parent.verticalCenter
        color: mouse.pressed ? "white" : "black"
        font.pointSize: 8
        elide: Text.ElideRight
    }
    // 1px mid-grey line, inset a little from both sides (the library app dithers the same
    // tone as a dotted line)
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: Units.dp(4)    // 8px
        anchors.rightMargin: Units.dp(4)
        height: 1
        color: "#7f7f7f"
        visible: item.showSeparator && !mouse.pressed
    }
    MouseArea {
        id: mouse
        anchors.fill: parent
        onClicked: item.clicked()
    }
}
