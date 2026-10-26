import QtQuick

// An author's photo, filling the item (covers.authorPhoto() crops it to the size, as
// BookOrbit's portraits are), or without one their initials, as BookOrbit shows them: in
// a serif face on a gradient picked by their name, near the accent's hue
// (bookOrbit.authorMonogram()). The initials are underneath, so they show while the photo
// downloads, and if it fails. The caller sets the size; 3:4 suits most photos.
Item {
    id: portrait
    // A row of bookOrbit.authorsList, or bookOrbit.authorRow(): id, name, photo
    property var author: ({})

    // Again when the accent changes
    readonly property var monogram: bookOrbit.accentColor, bookOrbit.authorMonogram(author.name || "")

    // The gradient runs top left to bottom right, as CSS's 135deg does: a square with a
    // top-to-bottom gradient, turned, whose ends just reach those corners
    Item {
        anchors.fill: parent
        clip: true
        Rectangle {
            anchors.centerIn: parent
            width: (portrait.width + portrait.height) / Math.SQRT2
            height: width
            rotation: -45
            gradient: Gradient {
                GradientStop { position: 0; color: portrait.monogram.from }
                GradientStop { position: 1; color: portrait.monogram.to }
            }
        }
    }
    Text {
        anchors.centerIn: parent
        text: portrait.monogram.initials
        color: portrait.monogram.color
        // 36% of the width, as BookOrbit's are
        font.family: "PT Serif"
        font.bold: true
        font.pixelSize: Math.max(1, Math.round(portrait.width * 0.36))
    }

    Image {
        id: photo
        anchors.fill: parent
        // Bumped to ask again when the download arrives
        property int revision: 0
        source: revision >= 0 && (portrait.author.photo || "") !== ""
                ? covers.authorPhoto(portrait.author.id, portrait.author.photo, portrait.width, portrait.height) : ""
        Connections {
            target: covers
            function onPhotoReady(authorId) {
                if (authorId === portrait.author.id)
                    photo.revision++
            }
        }
        asynchronous: true
    }
}
