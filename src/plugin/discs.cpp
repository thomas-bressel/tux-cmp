#include "discs.h"

#include <QCoreApplication>
#include <QMetaObject>
#include <QObject>

#include "cpc/amsdos.h"
#include "plugin.h"
#include "prompts.h"

namespace tuxcmp::discs {

namespace {

QObject* tuxDskObject(tuxape::PluginHost* host)
{
    return host ? host->plugin(QString::fromLatin1(kTuxDskId)) : nullptr;
}

bool offers(QObject* object, const char* signature)
{
    return object && object->metaObject()->indexOfMethod(QMetaObject::normalizedSignature(signature).constData()) >= 0;
}

// What this file says, translated under its own name.
struct Texts {
    Q_DECLARE_TR_FUNCTIONS(tuxcmp::discs)
};

}  // namespace

TuxDskState tuxDsk(tuxape::PluginHost* host)
{
    QObject* object = tuxDskObject(host);
    if (!object)
        return TuxDskState::Missing;
    return offers(object, kPutFile) ? TuxDskState::Ready : TuxDskState::TooOld;
}

QString tuxDskDisc(tuxape::PluginHost* host)
{
    QObject* object = tuxDskObject(host);
    QString name;
    if (offers(object, kDiscName))
        QMetaObject::invokeMethod(object, "discName", Qt::DirectConnection, Q_RETURN_ARG(QString, name));
    return name;
}

QChar driveLetter(int drive)
{
    return QLatin1Char(static_cast<char>('A' + drive));
}

QString describe(const DiscTarget& target)
{
    return target.kind == DiscTarget::TuxDsk ? Texts::tr("TuxDSK's disc") : Texts::tr("the disc in drive %1:").arg(driveLetter(target.drive));
}

bool put(tuxape::PluginHost* host, const DiscTarget& target, const QString& name, const QByteArray& content,
         bool replace, QWidget* over)
{
    const auto proper = tuxcmp::cpc::discName(name.toStdString());
    if (!proper) {
        prompts::problem(over, Texts::tr("%1 is not a name a file can have on a disc.").arg(name));
        return false;
    }
    const QString file = QString::fromStdString(*proper);
    if (!host) {
        prompts::problem(over, Texts::tr("TuxAPE is not there to put %1 on a disc.").arg(file));
        return false;
    }

    if (target.kind == DiscTarget::TuxDsk) {
        QObject* object = tuxDskObject(host);
        if (!object) {
            prompts::problem(over, Texts::tr("TuxDSK is not loaded: install it, or turn it on, in TuxAPE's Settings, on the Plugins page."));
            return false;
        }
        bool done = false;
        if (!offers(object, kPutFile)
            || !QMetaObject::invokeMethod(object, "putFile", Qt::DirectConnection, Q_RETURN_ARG(bool, done),
                                          Q_ARG(QString, file), Q_ARG(QByteArray, content), Q_ARG(bool, replace))) {
            prompts::problem(over, Texts::tr("This version of TuxDSK cannot take files from other plugins: install a newer one."));
            return false;
        }
        // TuxDSK has said why, if it did not take the file.
        return done;
    }

    const tuxape::PluginDisc disc = host->disc(target.drive);
    if (!disc.present) {
        prompts::problem(over, Texts::tr("There is no disc in drive %1: of TuxAPE. Insert one, or make a blank one, from its File menu.")
                                   .arg(driveLetter(target.drive)));
        return false;
    }
    if (!replace && disc.files.contains(file)
        && !prompts::ask(over, QString(), Texts::tr("%1 is already on %2. Replace it?").arg(file, describe(target))))
        return false;
    const QString error = host->putFile(target.drive, file, content);
    if (!error.isEmpty()) {
        prompts::problem(over, error);
        return false;
    }
    return true;
}

}  // namespace tuxcmp::discs
