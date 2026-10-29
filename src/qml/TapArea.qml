import QtQuick
import "units.js" as Units

// A MouseArea for list rows that sends tapped() only for a deliberate tap:
//  - The finger lifts near where it pressed. The list takes over a swipe once it sees the
//    finger move, but if the touch screen sends few move events (as when the device is
//    busy), a swipe can arrive as a press and a distant release.
//  - The press isn't during or just after a scroll. Lifting the finger off a scroll that
//    stops with it can register as a tap.
MouseArea {
    id: area
    signal tapped()

    // The list the row is in, if any
    property Flickable list: null

    property point pressedAt
    property bool pressedScrolling: false
    // When the list last stopped, from Date.now()
    property double listStoppedAt: 0
    readonly property int settleMs: 300

    Connections {
        target: area.list
        function onMovementEnded() { area.listStoppedAt = Date.now() }
    }

    onPressed: (mouse) => {
        pressedAt = Qt.point(mouse.x, mouse.y)
        pressedScrolling = list !== null && (list.moving || Date.now() - listStoppedAt < settleMs)
    }
    onClicked: (mouse) => {
        const moved = Math.hypot(mouse.x - pressedAt.x, mouse.y - pressedAt.y)
        if (pressedScrolling)
            diagnostics.trace("tap ignored: " + (Date.now() - listStoppedAt) + " ms after scrolling")
        else if (moved > Units.dp(12))
            diagnostics.trace("tap ignored: moved " + Math.round(moved) + "px")
        else
            tapped()
    }
}
