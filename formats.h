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

// "ebook" or "audiobook"
inline QString mediaKind(const QString &format)
{
    return isAudioFormat(format) ? QStringLiteral("audiobook") : QStringLiteral("ebook");
}
