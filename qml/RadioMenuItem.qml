import QtQuick
import "units.js" as Units

// Option row in an options menu, after Libby's filter menu on an Era Color: radio button
// centred 87px from the menu's border, label at 143px, grey line underneath inset 33px.
// Inverted while pressed.
Item {
    id: item
    property alias text: label.text
    property bool checked: false
    property bool showSeparator: true
    signal clicked()

    width: parent ? parent.width : 0
    implicitWidth: label.x + label.implicitWidth + Units.dp(15)
    height: Units.dp(50)   // 109px

    Rectangle {
        anchors.fill: parent
        color: "black"
        visible: mouse.pressed
    }
    Image {
        x: Units.dp(40) - width / 2
        anchors.verticalCenter: parent.verticalCenter
        source: "image://theme/" + (item.checked ? "radiobtn_checked_s" : "radiobtn_s")
                + (mouse.pressed ? "@inverted" : "")
    }
    Text {
        id: label
        x: Units.dp(66)
        anchors.verticalCenter: parent.verticalCenter
        color: mouse.pressed ? "white" : "black"
        font.pointSize: 8
    }
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: Units.dp(15)
        anchors.rightMargin: Units.dp(15)
        height: 1
        color: "#777777"
        visible: item.showSeparator && !mouse.pressed
    }
    MouseArea {
        id: mouse
        anchors.fill: parent
        onClicked: item.clicked()
    }
}
