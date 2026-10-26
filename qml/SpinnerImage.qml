import QtQuick

// One of the system's spinners, turning while visible: theme icons <frames>1 to 9, e.g.
// loading_icon_ (big, for a page) or progress_indicator_ (small, for a button)
Image {
    id: spinner
    property string frames: "loading_icon_"
    property int frame: 1

    source: "image://theme/" + frames + frame
    Timer {
        interval: 200
        repeat: true
        running: spinner.visible
        onTriggered: spinner.frame = spinner.frame % 9 + 1
    }
}
