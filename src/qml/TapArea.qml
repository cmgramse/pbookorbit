import QtQuick
import "units.js" as Units

// A MouseArea for list rows that sends tapped() only if the finger lifts near where it
// pressed. The list takes over a swipe once it sees the finger move, but if the touch
// screen sends few move events (as when the device is busy), a swipe can arrive as a press
// and a distant release, which MouseArea alone would count as a click.
MouseArea {
    signal tapped()

    property point pressedAt

    onPressed: (mouse) => pressedAt = Qt.point(mouse.x, mouse.y)
    onClicked: (mouse) => {
        const moved = Math.hypot(mouse.x - pressedAt.x, mouse.y - pressedAt.y)
        if (moved <= Units.dp(12))
            tapped()
        else
            diagnostics.trace("tap ignored: moved " + Math.round(moved) + "px")
    }
}
