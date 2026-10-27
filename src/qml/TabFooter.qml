import QtQuick
import "units.js" as Units

// Footer for switching between views, after the Libby app on an Era Color: a full-width
// line, then equal tabs (inset 33px from the sides), each a theme icon over its label.
// The current tab's label sits in a black pill.
Rectangle {
    id: footer
    // [{ label: "Library", icon: "overdrive_library" }, ...]
    property var tabs: []
    property int currentIndex: 0
    signal tabClicked(int index)

    color: "white"
    height: 1 + Units.dp(1) + tabRow.height + Units.dp(7)

    Rectangle {
        width: parent.width
        height: 1
        color: "black"
    }

    Row {
        id: tabRow
        x: Units.dp(15)
        y: 1 + Units.dp(1)
        width: parent.width - 2 * x

        Repeater {
            model: footer.tabs
            delegate: Item {
                id: tab
                required property var modelData
                required property int index
                readonly property bool current: index === footer.currentIndex

                width: tabRow.width / Math.max(1, footer.tabs.length)
                height: column.height

                Column {
                    id: column
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: Units.dp(2)

                    Image {
                        anchors.horizontalCenter: parent.horizontalCenter
                        source: "image://theme/" + tab.modelData.icon
                    }
                    // 143x47px pill for "Library"
                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: label.implicitWidth + 2 * Units.dp(12)
                        height: Units.dp(22)
                        radius: height / 2
                        color: tab.current ? "black" : "white"

                        Text {
                            id: label
                            anchors.centerIn: parent
                            text: tab.modelData.label
                            color: tab.current ? "white" : "black"
                            font.pointSize: 6
                            font.bold: true
                        }
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: footer.tabClicked(tab.index)
                }
            }
        }
    }
}
