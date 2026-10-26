import QtQuick
import "units.js" as Units

// One row of a PopupMenu: optional theme icon, then the label. While pressed the row
// gets a black rounded fill and the icon and label turn white. A grey line separates it
// from the row below. Measured from the library and Sudoku apps on an Era Color.
//
// A row that opens a submenu in place, like the library's Open with (screenshot 16), has
// an arrow at its right; the submenu's rows below it are subItems: no icon, label and
// line indented.
Item {
    id: item
    property alias text: label.text
    property string icon            // optional theme resource name, e.g. "logout"
    property bool showSeparator: true
    property string arrow: ""       // "up" or "down" for a row that opens a submenu
    property bool subItem: false
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
        // Label at 112px with an icon, 39px without, 83px in a submenu
        anchors.leftMargin: item.icon !== "" ? Units.dp(52) : item.subItem ? Units.dp(38) : Units.dp(18)
        anchors.right: parent.right
        anchors.rightMargin: Units.dp(10) + (item.arrow !== "" ? arrow.width + arrow.anchors.rightMargin : 0)   // 21px
        anchors.verticalCenter: parent.verticalCenter
        color: mouse.pressed ? "white" : "black"
        font.pointSize: 8
        elide: Text.ElideRight
    }
    // The theme's 21px arrow, 9px from the row's right edge
    Image {
        id: arrow
        anchors.right: parent.right
        anchors.rightMargin: Units.dp(4)
        anchors.verticalCenter: parent.verticalCenter
        visible: item.arrow !== ""
        source: item.arrow === "" ? "" : "image://theme/arrow_" + item.arrow + (mouse.pressed ? "@inverted" : "")
    }
    // 1px mid-grey line, inset a little from both sides (the library app dithers the same
    // tone as a dotted line); in a submenu 59px from the left and 33px from the right
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: item.subItem ? Units.dp(27) : Units.dp(4)    // 8px
        anchors.rightMargin: item.subItem ? Units.dp(15) : Units.dp(4)
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
