import QtQuick
import "units.js" as Units

// On/off row in an options menu, for a lesser option that goes with the others rather
// than instead of them: the label on the left, lined up with the radio buttons above, and
// the system's off/on pill switch on the right, as in the system settings. A black line
// closes the group above, like the line under a MenuHeading, with a gap before the row.
// Smaller than the radio rows (8pt, the switch at 3/4 size) so it stays in the background.
// The row below the line is the tap target, inverted while pressed.
Item {
    id: item
    property alias text: label.text
    property bool checked: false
    signal clicked()

    // Where the radio buttons' circles start (their 74px image has a margin round it)
    readonly property int sideMargin: Units.dp(30)   // 65px

    readonly property int gap: Units.dp(8)   // 17px between the line and the row

    width: parent ? parent.width : 0
    implicitWidth: 2 * sideMargin + label.implicitWidth + Units.dp(15) + toggle.width
    height: gap + Units.dp(42)   // the row 91px, against the radio rows' 109px

    // The row itself, below the line and the gap
    Item {
        id: row
        y: item.gap
        width: parent.width
        height: parent.height - y

        Rectangle {
            anchors.fill: parent
            color: "black"
            visible: mouse.pressed
        }
        Text {
            id: label
            x: item.sideMargin
            anchors.verticalCenter: parent.verticalCenter
            color: mouse.pressed ? "white" : "black"
            font.pointSize: 8
        }
        // The theme's 145x61px switch at 3/4 size
        Image {
            id: toggle
            anchors.right: parent.right
            anchors.rightMargin: item.sideMargin
            anchors.verticalCenter: parent.verticalCenter
            width: Units.dp(50)    // 109px
            height: Units.dp(21)   // 46px
            sourceSize.width: width
            sourceSize.height: height
            source: "image://theme/" + (item.checked ? "switch_on" : "switch_off")
                    + (mouse.pressed ? "@inverted" : "")
        }
        MouseArea {
            id: mouse
            anchors.fill: parent
            onClicked: item.clicked()
        }
    }
    // Black line over the row, inset as a MenuHeading's
    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: Units.dp(15)
        anchors.rightMargin: Units.dp(15)
        height: 1
        color: "black"
    }
}
