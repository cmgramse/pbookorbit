import QtQuick
import "units.js" as Units

// Bar under the toolbar with a filter button on the left and a sort button on the right,
// modelled on Libby on an Era Color. While its menu is open, a button becomes a black tab
// running down over the menu's top border so they read as one shape. The down arrow after
// each label marks a menu. As in the system library, the sort icon is a separate round
// button that reverses the sort and shows its direction; it isn't part of the tab, so it
// doesn't change while the menu is open.
Item {
    id: bar
    property string filterText
    property string sortText
    property bool filterOpen: false
    property bool sortOpen: false
    property bool sortReversed: false
    // Sort-only pages leave out the filter button
    property bool showFilter: true
    // How far an open tab reaches below the bar: the menu border width
    property int tabOverhang: Units.dp(4)
    // For a copy drawn over the faded page while a menu is open, showing just the open tab
    property bool onlyOpenTab: false
    signal filterClicked()
    signal sortClicked()
    signal sortIconClicked()

    // Where menus go, in this item's coordinates
    readonly property real filterMenuX: filterButton.x
    readonly property real sortMenuRight: sortButton.x + sortButton.width
    readonly property real menuY: height - 1

    height: Units.dp(55)

    // A button: icon, label and arrow
    component DropdownButton: Item {
        id: button
        property string icon
        property string text
        property bool capitals: false
        property bool open: false
        property int spacing: Units.dp(2)
        // Whether the icon is a separate button outside the tab. Its taps send iconClicked
        // instead of clicked, and it shows pressedIcon while pressed.
        property bool iconIsButton: false
        property string pressedIcon
        signal clicked()
        signal iconClicked()

        width: content.width + Units.dp(8)
        height: bar.height

        // The tab, without a separate icon button
        Rectangle {
            x: button.iconIsButton ? content.x + label.x - Units.dp(4) : 0
            width: parent.width - x
            height: parent.height + bar.tabOverhang
            color: "black"
            visible: button.open
        }
        Row {
            id: content
            anchors.verticalCenter: parent.verticalCenter
            spacing: button.spacing
            Image {
                id: iconImage
                anchors.verticalCenter: parent.verticalCenter
                // Left out of the open-tab copy so the faded one beneath shows
                opacity: button.iconIsButton && bar.onlyOpenTab ? 0 : 1
                source: "image://theme/" + (!button.iconIsButton ? button.icon + (button.open ? "@inverted" : "")
                                            : iconMouse.pressed ? button.pressedIcon : button.icon)
            }
            Text {
                id: label
                anchors.verticalCenter: parent.verticalCenter
                text: button.text + " ↓"
                color: button.open ? "white" : "black"
                font.pointSize: 8
                font.capitalization: button.capitals ? Font.AllUppercase : Font.MixedCase
            }
        }
        MouseArea {
            anchors.fill: parent
            onClicked: button.clicked()
        }
        // Icon tap area: full height, from the button's left edge to halfway into the gap
        // before the label
        MouseArea {
            id: iconMouse
            visible: button.iconIsButton && !bar.onlyOpenTab
            width: content.x + iconImage.width + button.spacing / 2
            height: parent.height
            onClicked: button.iconClicked()
        }
    }

    // 1px line, inset from both sides
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
        visible: bar.showFilter && (!bar.onlyOpenTab || open)
        onClicked: bar.filterClicked()
    }
    DropdownButton {
        id: sortButton
        x: bar.width - Units.dp(26) - width
        icon: bar.sortReversed ? "sorting_ascending" : "sorting_descending"
        text: bar.sortText
        spacing: Units.dp(10)
        open: bar.sortOpen
        iconIsButton: true
        pressedIcon: icon + "_inv"
        visible: !bar.onlyOpenTab || open
        onClicked: bar.sortClicked()
        onIconClicked: bar.sortIconClicked()
    }
}
