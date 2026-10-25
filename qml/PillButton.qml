import QtQuick
import "units.js" as Units

// Button in the style of PocketBook's own apps, measured from the OverDrive sign-in
// screen on an Era Color: pill-shaped, 121px tall, 1px pure black border, about 77px
// between border and content, label in capitals, optional theme icon on the left.
// Inverted while pressed.
Rectangle {
    id: button
    property alias text: label.text
    property string icon            // optional theme resource name, e.g. "search"
    property string pressedIcon     // shown while pressed, e.g. "search_inv"
    signal clicked()

    readonly property int padding: Units.dp(36)

    implicitWidth: Math.max(Units.dp(200), content.implicitWidth + 2 * padding)
    implicitHeight: Units.dp(56)
    radius: height / 2
    // One device pixel, like the native buttons, so not scaled with dp()
    border.width: 1
    border.color: "black"
    color: mouse.pressed ? "black" : "white"
    opacity: enabled ? 1 : 0.4

    Row {
        id: content
        anchors.centerIn: parent
        spacing: Units.dp(12)

        Image {
            anchors.verticalCenter: parent.verticalCenter
            visible: button.icon !== "" && status === Image.Ready
            source: button.icon === "" ? ""
                  : "image://theme/" + (mouse.pressed && button.pressedIcon ? button.pressedIcon : button.icon)
        }
        Text {
            id: label
            anchors.verticalCenter: parent.verticalCenter
            color: mouse.pressed ? "white" : "black"
            font.pointSize: 8
            font.capitalization: Font.AllUppercase
        }
    }
    MouseArea {
        id: mouse
        anchors.fill: parent
        onClicked: button.clicked()
    }
}
