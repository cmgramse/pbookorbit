import QtQuick

// What the system library shows for a book with no cover, used here while covers download
// too: a theme icon scaled to fit. e3_list_book (outline with title lines) suits list rows,
// book_card_dummy_cover_land the book info page.
Image {
    property string icon: "e3_list_book"

    anchors.centerIn: parent
    sourceSize: Qt.size(parent.width, parent.height)
    source: "image://theme/" + icon
}
