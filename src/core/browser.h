#pragma once

#include <QObject>

// Opens web pages in the system's browser, as KOReader does on PocketBook: OpenBook()
// with the browser app and the URL. Only with multitasking, so the app is still there
// when the browser closes.
//
// The emulator has no browser, so there it only logs the URL, so that links can be tried.
class Browser : public QObject
{
    Q_OBJECT
    // Whether there is a browser to open pages in
    Q_PROPERTY(bool available READ available CONSTANT)

public:
    explicit Browser(QObject *parent = nullptr);
    bool available() const { return !m_app.isEmpty(); }

    // Opens an http(s) web address, adding https:// to one without a scheme
    // ("example.com"). Anything else is refused.
    Q_INVOKABLE bool open(const QString &address);

private:
    QString m_app;
};
