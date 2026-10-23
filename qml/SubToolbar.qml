import QtQuick
import "units.js" as Units

// Bar under the toolbar with a filter button on the left and a sort button on the right,
// after the Libby app on an Era Color. A button whose menu is open becomes a black tab
// that runs down over the menu's top border, so the two read as one shape.
Item {
    id: bar
    property string filterText
    property string sortText
    property bool filterOpen: false
    property bool sortOpen: false
    // How far an open tab reaches below the bar: the menu's border width
    property int tabOverhang: Units.dp(4)
    // For a copy drawn over the faded page while a menu is open: just the open tab
    property bool onlyOpenTab: false
    signal filterClicked()
    signal sortClicked()

    // Where the menus go, in this item's coordinates
    readonly property real filterMenuX: filterButton.x
    readonly property real sortMenuRight: sortButton.x + sortButton.width
    readonly property real menuY: height - 1

    height: Units.dp(55)   // 120px

    // One of the two buttons: icon, label and an arrow showing whether its menu is open
    component DropdownButton: Item {
        id: button
        property string icon
        property string text
        property bool capitals: false
        property bool open: false
        property int spacing: Units.dp(2)
        signal clicked()

        width: content.width + Units.dp(8)
        height: bar.height

        Rectangle {
            width: parent.width
            height: parent.height + bar.tabOverhang
            color: "black"
            visible: button.open
        }
        Row {
            id: content
            anchors.verticalCenter: parent.verticalCenter
            spacing: button.spacing
            Image {
                anchors.verticalCenter: parent.verticalCenter
                source: "image://theme/" + button.icon + (button.open ? "@inverted" : "")
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: button.text + (button.open ? " ↑" : " ↓")
                color: button.open ? "white" : "black"
                font.pointSize: 9
                font.capitalization: button.capitals ? Font.AllUppercase : Font.MixedCase
            }
        }
        MouseArea {
            anchors.fill: parent
            onClicked: button.clicked()
        }
    }

    // 1px line, inset 56px from both sides
    Rectangle {
        visible: !bar.onlyOpenTab
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: Units.dp(26)
        anchors.rightMargin: Units.dp(26)
        height: 1
        color: "black"
    }

    DropdownButton {
        id: filterButton
        x: Units.dp(26)
        icon: "filter_empty"
        text: bar.filterText
        capitals: true
        open: bar.filterOpen
        visible: !bar.onlyOpenTab || open
        onClicked: bar.filterClicked()
    }
    DropdownButton {
        id: sortButton
        x: bar.width - Units.dp(26) - width
        icon: "sorting_descending"
        text: bar.sortText
        spacing: Units.dp(10)
        open: bar.sortOpen
        visible: !bar.onlyOpenTab || open
        onClicked: bar.sortClicked()
    }
}
