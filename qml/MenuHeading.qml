import QtQuick
import "units.js" as Units

// Section heading inside an options menu (Libby's "CONTENT TYPE"): grey capitals,
// centred, over a black line inset 33px from the menu's border.
Item {
    property alias text: label.text

    width: parent ? parent.width : 0
    implicitWidth: label.implicitWidth + 2 * Units.dp(15)
    height: Units.dp(50)   // 109px

    Text {
        id: label
        anchors.centerIn: parent
        color: "#606060"
        font.pointSize: 9
        font.capitalization: Font.AllUppercase
    }
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: Units.dp(15)
        anchors.rightMargin: Units.dp(15)
        height: 1
        color: "black"
    }
}
