#pragma once

#include <QString>
#include <QStringList>

// Audiobook formats, as BookOrbit lists them (AUDIO_FORMAT_LIST); anything else with a
// format is an ebook
inline bool isAudioFormat(const QString &format)
{
    static const QStringList audio{"m4b", "mp3", "m4a", "opus", "ogg", "flac"};
    return audio.contains(format.toLower());
}

// Formats of book files, ebooks or audiobooks, as BookOrbit takes them
inline bool isBookFormat(const QString &format)
{
    static const QStringList ebook{"epub", "kepub", "mobi", "azw3", "azw", "fb2", "pdf", "djvu", "cbz", "cbr"};
    return ebook.contains(format.toLower()) || isAudioFormat(format);
}

// "ebook" or "audiobook"
inline QString mediaKind(const QString &format)
{
    return isAudioFormat(format) ? QStringLiteral("audiobook") : QStringLiteral("ebook");
}
