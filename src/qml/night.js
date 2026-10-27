// Pictures and colours that keep their real colours in night mode. The screen inverts
// everything the app draws while night mode is on, so these are drawn inverted for it to
// turn back, as the system's own apps do: covers, photos and the accent stay as they are
// while the page goes dark. eink.nightMode comes from main.cpp.

// An image's URL: in night mode the inverted copy from InvertedImageProvider
function image(url) {
    return url !== "" && eink.nightMode ? "image://inverted/" + url : url
}

// A colour: in night mode its inverse
function color(c) {
    return eink.nightMode ? Qt.rgba(1 - c.r, 1 - c.g, 1 - c.b, c.a) : c
}
