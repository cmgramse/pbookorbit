#pragma once

#include <QColor>
#include <QImage>

// Colour as the system's own apps prepare it for the Era Color's colour e-ink, which
// shows colours much paler than a normal screen: InkView's adjustAreaDefault(), which the
// system library uses for covers (tested on the device in October 2026; see NOTES.md). On
// the device it saturates and brightens strongly and dithers to the screen's colours; in
// the emulator it hardly changes anything, so the emulator can't show it.

// Every pixel of the image adjusted for the screen, dithering included, so draw it at its
// own size and keep it out of lossy formats. Converts it to RGB32.
void adjustForEink(QImage &image);

// A flat colour adjusted for the screen: the average of a patch of it after
// adjustForEink(), for fills where a dither pattern would look out of place
QColor einkColor(const QColor &color);
