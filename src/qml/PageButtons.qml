import QtQuick
import "units.js" as Units

// Page buttons over the bottom corners of a list, as in the system library (screenshot 9):
// up on the left, down on the right, each scrolling the list by a screenful. Paging suits
// e-ink better than flicking: one clean redraw per page. The buttons are half-white, fading
// the rows behind them (black shows as 128 grey in the screenshot), with a 1px line along
// the top and the inner side, rounded where they meet. Goes in the list's parent with its
// bottom on the list's bottom; between the buttons taps reach the list.
Item {
    id: buttons
    property ListView list

    // Each page keeps this much of the last one, the strip the buttons sat over
    readonly property int overlap: height

    visible: list.visible && list.contentHeight > list.height
    width: parent.width
    height: Units.dp(57)   // 124px

    // A ListView's origin moves as it lays out rows it hadn't measured, so a page reaching
    // either end goes to that end the ListView's own way rather than to a computed position
    function page(direction) {
        const top = list.originY
        const bottom = list.originY + list.contentHeight - list.height
        const target = list.contentY + direction * (list.height - overlap)
        if (target <= top)
            list.positionViewAtBeginning()
        else if (target >= bottom)
            list.positionViewAtEnd()
        else
            list.contentY = target
    }

    component PageButton: Item {
        id: button
        // The inner side, which has the line and the rounded corner
        property bool innerLeft
        property alias icon: image.source
        signal clicked()

        width: Units.dp(84)   // 182px
        height: parent.height
        // The outer side and the bottom have no line
        clip: true

        Rectangle {
            readonly property int hidden: radius + 1
            x: button.innerLeft ? 0 : -hidden
            width: button.width + hidden
            height: button.height + hidden
            radius: Units.dp(6)   // 12px
            color: Qt.rgba(1, 1, 1, 0.5)
            border.width: 1
            border.color: "black"
        }
        Image {
            id: image
            anchors.centerIn: parent
        }
        MouseArea {
            anchors.fill: parent
            onClicked: button.clicked()
        }
    }

    PageButton {
        anchors.left: parent.left
        icon: "image://theme/arrow_page_prev"
        onClicked: buttons.page(-1)
    }
    PageButton {
        anchors.right: parent.right
        innerLeft: true
        icon: "image://theme/arrow_page_next"
        onClicked: buttons.page(1)
    }
}
