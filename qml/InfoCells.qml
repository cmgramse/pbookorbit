import QtQuick
import "units.js" as Units

// Three cells side by side on an info page (BookInfoPage, AuthorInfoPage), a grey capital
// label over each value, split by grey lines; cells without a value are left blank, and
// the row goes if all of them are. A cell given a third entry, true, can be tapped: it
// has a chevron at its right, is inverted while pressed, as buttons are, and sends
// cellClicked(index).
Item {
    id: cellRow
    property var cells: []
    property bool showTopLine: true
    signal cellClicked(int index)
    visible: cells.some(cell => cell[1] !== "")
    height: visible ? Units.dp(82) : 0     // 969 to 1148px

    Rectangle {
        visible: cellRow.showTopLine
        width: parent.width
        height: 1
        color: "#7f7f7f"
    }
    Row {
        anchors.fill: parent
        Repeater {
            model: cellRow.cells
            Item {
                id: cell
                required property var modelData
                required property int index
                readonly property bool tappable: modelData.length > 2 && modelData[2] === true
                width: cellRow.width / 3
                height: cellRow.height

                // Pressed: black, between the lines
                Rectangle {
                    visible: mouse.pressed
                    x: index > 0 ? 1 : 0
                    y: 1
                    width: parent.width - x
                    height: parent.height - 2
                    color: "black"
                }

                // Line before every cell but the first, short of the row's top and bottom
                Rectangle {
                    visible: index > 0
                    y: Units.dp(13)
                    width: 1
                    height: parent.height - 2 * Units.dp(13)
                    color: "#7f7f7f"
                }
                Column {
                    x: Units.dp(11)
                    // Clear of the chevron
                    width: parent.width - 2 * x - (cell.tappable ? chevron.width + Units.dp(6) : 0)
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: Units.dp(8)
                    Text {
                        width: parent.width
                        text: modelData[0]
                        color: mouse.pressed ? "#c0c0c0" : "#808080"
                        font.pointSize: 8
                        font.capitalization: Font.AllUppercase
                        elide: Text.ElideRight
                    }
                    Text {
                        width: parent.width
                        text: modelData[1]
                        color: mouse.pressed ? "white" : "black"
                        font.pointSize: 8
                        elide: Text.ElideRight
                    }
                }
                // The toolbar's back chevron, turned to point right: the theme has no
                // forward one that size. Its own padding keeps it off the cell's edge; its
                // back_inv is a disc, so pressed it is the plain one inverted.
                Image {
                    id: chevron
                    visible: cell.tappable
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    source: !cell.tappable ? "" : mouse.pressed ? "image://theme/back@inverted" : "image://theme/back"
                    mirror: true
                }
                MouseArea {
                    id: mouse
                    anchors.fill: parent
                    enabled: cell.tappable
                    onClicked: cellRow.cellClicked(cell.index)
                }
            }
        }
    }
    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: "#7f7f7f"
    }
}
