import QtQuick
import "units.js" as Units

// On/off row in an options menu, for a secondary option that combines with the others
// rather than replacing them: label on the left, aligned with the radio buttons above, and
// the system's pill switch on the right, as in system settings. A black line closes the
// group above (like under a MenuHeading), with a gap before the row. Smaller than the radio
// rows (8pt, switch at 3/4 size) to keep it low-key. The row below the line is the tap
// target, inverted while pressed.
Item {
    id: item
    property alias text: label.text
    property bool checked: false
    signal clicked()

    // Where the radio circles start (their image has a margin)
    readonly property int sideMargin: Units.dp(30)

    readonly property int gap: Units.dp(8)   // between the line and the row

    width: parent ? parent.width : 0
    implicitWidth: 2 * sideMargin + label.implicitWidth + Units.dp(15) + toggle.width
    height: gap + Units.dp(42)   // shorter than the radio rows

    // The row itself, below the line and gap
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
            width: Units.dp(50)
            height: Units.dp(21)
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
    // Black line above the row, inset like a MenuHeading's
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
