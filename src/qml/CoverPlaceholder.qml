import QtQuick

// What the system library shows for a book without a cover, here also while a cover
// downloads: a theme icon, shrunk or grown to fit. e3_list_book (the outline with title
// lines) suits a list row; book_card_dummy_cover_land the book info page.
Image {
    property string icon: "e3_list_book"

    anchors.centerIn: parent
    sourceSize: Qt.size(parent.width, parent.height)
    source: "image://theme/" + icon
}
