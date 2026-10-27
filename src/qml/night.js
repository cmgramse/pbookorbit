// Images and colours that keep their real colours in night mode. Night mode inverts
// everything the app draws, so these are pre-inverted to come out right, as the system apps
// do: covers, photos and the accent stay put while the page goes dark. eink.nightMode is
// set in main.cpp.

// An image URL; in night mode, the inverted copy from InvertedImageProvider
function image(url) {
    return url !== "" && eink.nightMode ? "image://inverted/" + url : url
}

// A colour; in night mode, its inverse
function color(c) {
    return eink.nightMode ? Qt.rgba(1 - c.r, 1 - c.g, 1 - c.b, c.a) : c
}
