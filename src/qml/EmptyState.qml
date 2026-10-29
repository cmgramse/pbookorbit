import QtQuick
import "units.js" as Units

// What a page shows when it has nothing to list, or couldn't load: a light title with a
// short explanation under it, centred a little above the middle, as the system apps do
// (the Audiobooks app's "No audiobooks", screenshot 20). Fill the space below the page's
// toolbars with it.
Item {
    id: state
    property alias title: titleText.text
    property alias text: bodyText.text
    // The explanation for lists cut down to downloaded books while offline
    readonly property string offlineText: "You're currently offline, only downloaded books can be shown."

    Column {
        x: Units.dp(90)
        width: parent.width - 2 * x
        // The middle of the text 38% of the way down
        y: Math.round(parent.height * 0.38 - height / 2)

        Text {
            id: titleText
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            font.pointSize: 14
            font.weight: Font.Light
        }
        Text {
            id: bodyText
            visible: text !== ""
            width: parent.width
            topPadding: Units.dp(10)
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            lineHeight: 1.15
            font.pointSize: 8
        }
    }
}
