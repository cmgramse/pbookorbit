#include "collectionicons.h"

#include "bookorbitclient.h"
#include "diagnostics.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QUrl>

static const QString CUSTOM_PREFIX = "custom:";
// What BookOrbit's web client shows for a collection without a known icon
static const QString FALLBACK_ICON = "FolderOpen";

CollectionIcons::CollectionIcons(BookOrbitClient *client, const QString &cacheDir, QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_cacheDir(cacheDir)
{
    QDir().mkpath(m_cacheDir);
}

// "a-arrow-down" -> "AArrowDown", "building-2" -> "Building2": the names @lucide/vue exports
static QString pascalCase(const QString &kebab)
{
    QString name;
    for (const QString &part : kebab.split('-', Qt::SkipEmptyParts))
        name += part.left(1).toUpper() + part.mid(1);
    return name;
}

void CollectionIcons::loadLucide()
{
    m_lucideLoaded = true;
    QFile file(":/lucide/lucide-icons.json");
    if (!file.open(QIODevice::ReadOnly)) {
        diagnostics::log("lucide icons missing");
        return;
    }
    const QJsonObject set = QJsonDocument::fromJson(file.readAll()).object();
    m_lucideIcons = set.value("icons").toObject();
    for (auto it = m_lucideIcons.constBegin(); it != m_lucideIcons.constEnd(); ++it)
        m_lucideNames.insert(pascalCase(it.key()), it.key());
    const QJsonObject aliases = set.value("aliases").toObject();
    for (auto it = aliases.constBegin(); it != aliases.constEnd(); ++it)
        m_lucideNames.insert(pascalCase(it.key()), it.value().toString());
}

QString CollectionIcons::lucideFile(const QString &name)
{
    if (!m_lucideLoaded)
        loadLucide();
    const QString kebab = m_lucideNames.value(name);
    if (kebab.isEmpty())
        return QString();
    const QString path = QString("%1/lucide-%2.svg").arg(m_cacheDir, kebab);
    if (QFileInfo::exists(path))
        return path;

    // As lucide-static draws it, in black
    QString svg = "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"24\" height=\"24\" viewBox=\"0 0 24 24\" "
                  "fill=\"none\" stroke=\"#000000\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\">";
    for (const QJsonValue &node : m_lucideIcons.value(kebab).toArray()) {
        svg += '<' + node[0].toString();
        const QJsonObject attributes = node[1].toObject();
        for (auto it = attributes.constBegin(); it != attributes.constEnd(); ++it) {
            const QString value = it.value().isString() ? it.value().toString() : QString::number(it.value().toDouble());
            svg += QString(" %1=\"%2\"").arg(it.key(), value.toHtmlEscaped());
        }
        svg += "/>";
    }
    svg += "</svg>";
    QFile out(path);
    if (!out.open(QIODevice::WriteOnly) || out.write(svg.toUtf8()) < 0) {
        diagnostics::log("can't write " + path);
        return QString();
    }
    return path;
}

QString CollectionIcons::source(const QString &icon)
{
    if (!icon.startsWith(CUSTOM_PREFIX)) {
        QString path = lucideFile(icon);
        if (path.isEmpty())
            path = lucideFile(FALLBACK_ICON);
        return path.isEmpty() ? QString() : QUrl::fromLocalFile(path).toString();
    }

    // Only slugs BookOrbit allows: lower-case letters and digits, joined by hyphens
    static const QRegularExpression slugPattern("^[a-z0-9]+(?:-[a-z0-9]+)*$");
    const QString slug = icon.mid(CUSTOM_PREFIX.size());
    if (!slugPattern.match(slug).hasMatch())
        return QUrl::fromLocalFile(lucideFile(FALLBACK_ICON)).toString();
    const QString path = QString("%1/custom-%2.svg").arg(m_cacheDir, slug);
    if (!m_fetched.contains(slug)) {
        m_fetched.insert(slug);
        m_client->download(QString("/custom-icons/%1.svg").arg(slug),
            [this, icon, path](const QByteArray &data) {
                // The web client tints them with the text colour; here they are black
                QByteArray svg = data;
                svg.replace("currentColor", "#000000");
                QFile out(path);
                if (out.open(QIODevice::WriteOnly) && out.write(svg) >= 0) {
                    out.close();
                    emit ready(icon);
                }
            },
            [this, slug](int status, const QString &message) {
                m_fetched.remove(slug);   // try again next time it is shown
                diagnostics::log(QString("custom icon %1: %2 (%3)").arg(slug, message).arg(status));
            });
    }
    return QFileInfo::exists(path) ? QUrl::fromLocalFile(path).toString() : QString();
}
