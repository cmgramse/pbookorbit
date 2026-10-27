#pragma once

#include <QColor>
#include <QImage>

// Colour adjustment for the Era Color's e-ink, which shows colours much paler than an LCD.
// Wraps InkView's adjustAreaDefault(), which the system library uses for covers (tested on
// the device, October 2026; see NOTES.md). On the device it strongly saturates, brightens
// and dithers; in the emulator it barely changes anything.

// Adjusts every pixel for the screen, including dithering, so draw the result at its own
// size and don't store it lossily. Converts to RGB32.
void adjustForEink(QImage &image);

// A flat colour adjusted for the screen: the average of a patch after adjustForEink(), for
// fills where a dither pattern would look wrong
QColor einkColor(const QColor &color);
