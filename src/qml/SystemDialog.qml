import QtQuick
import "units.js" as Units

// A dialog laid out like the system's own (InkView's Dialog(), which can't be used from
// Qt), measured on an Era Color: thick black rounded border, a round theme icon on the
// left with the message beside it, and the buttons side by side along the bottom, split
// by thin grey lines. The window fades behind it, and a tap outside it does nothing: only
// its buttons close it.
//
// The message goes inside as children, laid out in a column.
Item {
    id: dialog
    // Theme icon: icon_question, icon_error, icon_info...
    property string icon: "icon_question"
    // The buttons, left to right: [label, action]
    property var buttons: []
    // How far something has got, 0-100, for a progress bar under the message; -1 for none
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
        // Near the bottom of the screen, as the system's dialogs are (screenshot 13): its
        // bottom edge 131px up
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Units.dp(60)
        width: parent.width - 2 * Units.dp(42)                 // 92px from each side
        height: body.height + buttonRow.height + 2 * border.width
        radius: Units.dp(9)
        border.width: Units.dp(4)                               // 9px
        border.color: "black"
        color: "white"

        // Icon and message
        Item {
            id: body
            x: panel.border.width
            y: panel.border.width
            width: panel.width - 2 * x
            height: Math.max(Units.dp(175), messageArea.height + 2 * Units.dp(25))   // 380px or more

            Image {
                id: icon
                x: Units.dp(25)                                 // 54px
                anchors.verticalCenter: parent.verticalCenter
                source: "image://theme/" + dialog.icon
            }

            Column {
                id: messageArea
                x: icon.x + icon.width + Units.dp(52)           // 113px after the icon
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

        // Buttons along the bottom: a grey line above them and between them, each
        // inverted while pressed
        Row {
            id: buttonRow
            x: panel.border.width
            y: body.y + body.height
            width: panel.width - 2 * x
            height: Units.dp(54)                                // 118px

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
                        // Square on top, rounded into the panel's corners below
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
                    // Line between this button and the one before
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
            x: buttonRow.x
            y: buttonRow.y
            width: buttonRow.width
            height: 1
            color: "#7f7f7f"
        }
    }
}
