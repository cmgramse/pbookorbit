#pragma once

#include <QObject>

// Opens web pages in the system browser, the way KOReader does on PocketBook: OpenBook()
// with the browser app and the URL. Needs multitasking, so the app is still there when the
// browser closes.
//
// The emulator has no browser, so there it just logs the URL.
class Browser : public QObject
{
    Q_OBJECT
    // Whether a browser is available
    Q_PROPERTY(bool available READ available CONSTANT)

public:
    explicit Browser(QObject *parent = nullptr);
    bool available() const { return !m_app.isEmpty(); }

    // Opens an http(s) address, adding https:// if there's no scheme ("example.com").
    // Refuses anything else.
    Q_INVOKABLE bool open(const QString &address);

private:
    QString m_app;
};
