#include "preferences.h"

#include <QDir>
#include <QSettings>

Preferences::Preferences(const QString& folder)
{
    if (folder.isEmpty())
        return;
    file_ = QDir(folder).filePath(QStringLiteral("TuxCMP.ini"));
    const QSettings kept(file_, QSettings::IniFormat);
    for (const QString& key : kept.allKeys())
        kept_.insert(key, kept.value(key));
}

void Preferences::set(const char* key, const QVariant& value)
{
    const QString name = QLatin1String(key);
    if (kept_.value(name) == value && kept_.contains(name))
        return;
    kept_.insert(name, value);
    if (file_.isEmpty())
        return;
    QSettings kept(file_, QSettings::IniFormat);
    kept.setValue(name, value);
    // Written at once: TuxAPE may unload the plugin at any time.
    kept.sync();
}
