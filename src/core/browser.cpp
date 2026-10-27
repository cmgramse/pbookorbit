#include "core/browser.h"
#include "core/diagnostics.h"

#include <inkview.h>
#include <QFileInfo>
#include <QUrl>

Browser::Browser(QObject *parent)
    : QObject(parent)
{
#ifdef EMULATOR
    m_app = "browser.app";
#else
    // Where firmware versions keep it, newest first
    static const char *const apps[] = {"/usr/bin/browser.app", "/ebrmain/bin/browser.app"};
    if (MultitaskingSupported()) {
        for (const char *app : apps) {
            if (QFileInfo::exists(app)) {
                m_app = app;
                break;
            }
        }
    }
#endif
}

bool Browser::open(const QString &address)
{
    QString text = address.trimmed();
    if (!text.contains("://"))
        text.prepend("https://");
    const QUrl url(text, QUrl::StrictMode);
    if (!available() || !url.isValid() || url.host().isEmpty()
            || (url.scheme() != "http" && url.scheme() != "https")) {
        diagnostics::log("can't open " + address);
        return false;
    }
    const QByteArray encoded = url.toEncoded();
#ifdef EMULATOR
    diagnostics::trace("would open " + QString::fromUtf8(encoded) + " in the browser");
    return true;
#else
    const int rc = OpenBook(m_app.toUtf8().constData(), encoded.constData(), 0);
    (rc < 0 ? diagnostics::log : diagnostics::trace)(QString("OpenBook %1 %2 -> %3").arg(m_app, QString::fromUtf8(encoded)).arg(rc));
    return rc >= 0;
#endif
}
