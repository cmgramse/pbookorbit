import QtQuick
import "units.js" as Units

// Toolbar of a page opened over the book list, after the system library's Book info page
// (screenshot 11): a back chevron at the left, the title centred in capitals, e.g.
// "BOOK INFO / 99 BOTTLES OF OOP", and a 1px line underneath.
Rectangle {
    id: header
    property alias title: titleText.text
    signal back()

    height: Units.dp(56) + 1
    color: "white"

    IconButton {
        id: backButton
        // Where the search page has it
        x: Units.dp(7)
        anchors.verticalCenter: parent.verticalCenter
        icon: "back"
        pressedIcon: "back_inv"
        fallbackText: "Back"
        onClicked: header.back()
    }

    Text {
        id: titleText
        anchors.centerIn: parent
        // Centred on the header, so leave the back button's width free on both sides
        width: parent.width - 2 * (backButton.x + backButton.width)
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
        font.pointSize: 8
        font.capitalization: Font.AllUppercase
    }

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: "black"
    }
}
