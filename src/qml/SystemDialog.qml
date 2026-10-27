import QtQuick
import "units.js" as Units

// A dialog styled like the system's own (InkView's Dialog(), which Qt apps can't use),
// measured on an Era Color: thick black rounded border, a round theme icon on the left with
// the message beside it, and buttons along the bottom split by thin grey lines. The window
// fades behind it, and only its buttons close it.
//
// Children form the message, laid out in a column.
Item {
    id: dialog
    // Theme icon: icon_question, icon_error, icon_info...
    property string icon: "icon_question"
    // Buttons left to right, as [label, action]. Leave empty for a status-only dialog.
    property var buttons: []
    // Progress 0-100 for a bar under the message, or -1 for none
    property int percent: -1
    default property alias content: message.data

    signal pressed(var action)

    visible: false

    // The faded window
    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(1, 1, 1, 0.6)
        MouseArea {
            anchors.fill: parent
        }
    }

    Rectangle {
        id: panel
        // Near the bottom of the screen, like the system's dialogs (screenshot 13)
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Units.dp(60)
        width: parent.width - 2 * Units.dp(42)
        height: body.height + buttonRow.height + 2 * border.width
        radius: Units.dp(9)
        border.width: Units.dp(4)
        border.color: "black"
        color: "white"

        // Icon and message
        Item {
            id: body
            x: panel.border.width
            y: panel.border.width
            width: panel.width - 2 * x
            height: Math.max(Units.dp(175), messageArea.height + 2 * Units.dp(25))

            Image {
                id: icon
                x: Units.dp(25)
                anchors.verticalCenter: parent.verticalCenter
                source: "image://theme/" + dialog.icon
            }

            Column {
                id: messageArea
                x: icon.x + icon.width + Units.dp(52)
                width: parent.width - x - Units.dp(25)
                anchors.verticalCenter: parent.verticalCenter
                spacing: Units.dp(6)

                Column {
                    id: message
                    width: parent.width
                    spacing: Units.dp(6)
                }
                // Progress: a black bar filling a 1px outline
                Rectangle {
                    width: parent.width
                    height: Units.dp(8)
                    visible: dialog.percent >= 0
                    border.width: 1
                    border.color: "black"
                    Rectangle {
                        width: parent.width * Math.max(0, dialog.percent) / 100
                        height: parent.height
                        color: "black"
                    }
                }
            }
        }

        // Buttons along the bottom, with grey lines above and between them, inverted while
        // pressed
        Row {
            id: buttonRow
            x: panel.border.width
            y: body.y + body.height
            width: panel.width - 2 * x
            height: dialog.buttons.length > 0 ? Units.dp(54) : 0

            Repeater {
                model: dialog.buttons
                Item {
                    required property var modelData
                    required property int index
                    width: buttonRow.width / dialog.buttons.length
                    height: buttonRow.height

                    Rectangle {
                        anchors.fill: parent
                        anchors.topMargin: 1
                        color: "black"
                        visible: press.pressed
                        // Square on top, rounded into the panel's corners at the bottom
                        bottomLeftRadius: index === 0 ? panel.radius - panel.border.width : 0
                        bottomRightRadius: index === dialog.buttons.length - 1 ? panel.radius - panel.border.width : 0
                    }
                    Text {
                        anchors.centerIn: parent
                        text: modelData[0]
                        color: press.pressed ? "white" : "black"
                        font.pointSize: 8
                        font.capitalization: Font.AllUppercase
                    }
                    // Divider before this button
                    Rectangle {
                        visible: index > 0
                        width: 1
                        height: parent.height
                        color: "#7f7f7f"
                    }
                    MouseArea {
                        id: press
                        anchors.fill: parent
                        onClicked: dialog.pressed(modelData[1])
                    }
                }
            }
        }
        // Line above the buttons
        Rectangle {
            visible: dialog.buttons.length > 0
            x: buttonRow.x
            y: buttonRow.y
            width: buttonRow.width
            height: 1
            color: "#7f7f7f"
        }
    }
}
