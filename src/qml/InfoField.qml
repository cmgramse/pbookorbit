import QtQuick
import "units.js" as Units

// One field on an info page (BookInfoPage, AuthorInfoPage): bold label over value with a
// grey line below; nothing if there's no value. A tappable field has a chevron on the
// right, inverts while pressed like InfoCells' cells, and sends clicked(). A link field
// (web address) underlines its value instead; only the text is tappable, inverting while
// pressed, and it sends linkClicked().
Item {
    id: field
    property string label
    property string value
    property bool richText: false
    property bool showLine: true
    property bool tappable: false
    property bool link: false
    signal clicked()
    signal linkClicked()
    visible: value !== ""
    implicitHeight: column.height

    // Pressed fill: black from the line above (halfway down the gap) to its own
    Rectangle {
        visible: mouse.pressed
        y: -Units.dp(9) + 1
        width: parent.width
        height: column.height - 1
        color: "black"
    }

    // Link text, with a black background while pressed
    Rectangle {
        visible: linkMouse.pressed
        x: valueText.x
        y: valueText.y
        width: valueText.contentWidth
        height: valueText.contentHeight
        color: "black"
    }

    Column {
        id: column
        width: parent.width
        spacing: Units.dp(6)

        Text {
            // Clear of the chevron
            width: parent.width - (field.tappable ? chevron.width + Units.dp(6) : 0)
            text: field.label
            color: mouse.pressed ? "white" : "black"
            font.pointSize: 8
            font.bold: true
            wrapMode: Text.Wrap
        }
        Text {
            id: valueText
            width: parent.width - (field.tappable ? chevron.width + Units.dp(6) : 0)
            text: field.value
            color: mouse.pressed || linkMouse.pressed ? "white" : "black"
            font.underline: field.link
            // Descriptions are HTML
            textFormat: field.richText ? Text.RichText : Text.PlainText
            font.pointSize: 8
            wrapMode: Text.Wrap
        }
        Item {
            width: parent.width
            height: Units.dp(18)
            Rectangle {
                visible: field.showLine
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width
                height: 1
                color: "#7f7f7f"
            }
        }
    }

    // As in InfoCells: the toolbar's back chevron turned to point right, level with the
    // label and value
    Image {
        id: chevron
        visible: field.tappable
        anchors.right: parent.right
        y: (valueText.y + valueText.height - height) / 2
        source: !field.tappable ? "" : mouse.pressed ? "image://theme/back@inverted" : "image://theme/back"
        mirror: true
    }
    MouseArea {
        id: linkMouse
        enabled: field.link
        x: valueText.x
        y: valueText.y
        width: valueText.contentWidth
        height: valueText.contentHeight
        onClicked: field.linkClicked()
    }
    MouseArea {
        id: mouse
        enabled: field.tappable
        y: -Units.dp(9)
        width: parent.width
        height: column.height
        onClicked: field.clicked()
    }
}
