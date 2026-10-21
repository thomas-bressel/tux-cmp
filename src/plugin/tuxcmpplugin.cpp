#include "tuxcmpplugin.h"

#include <QAction>
#include <QApplication>
#include <QEvent>

#include "compressorwindow.h"
#include "icons.h"

namespace tuxcmp {

bool TuxCmpPlugin::start(tuxape::PluginHost* host, QString*)
{
    host_ = host;
    // TuxCMP's texts in TuxAPE's language, before its first tr(): the
    // translations built into the library. None for English, the language
    // of the sources.
    if (translator_.load(QStringLiteral(":/i18n/tuxcmp/tuxcmp_%1.qm").arg(host->language())))
        QCoreApplication::installTranslator(&translator_);
    // One of the keys TuxAPE leaves to plugins (see PLUGINS.md).
    action_ = host->addAction(tr("TuxCMP – ZX0 Com&pressor..."), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F8));
    host->addButton(action_, compressIcon());
    connect(action_, &QAction::triggered, this, &TuxCmpPlugin::showWindow);
    // Qt tells the application when the look changes: its events are
    // watched for that one, which costs a comparison an event.
    qApp->installEventFilter(this);
    return true;
}

void TuxCmpPlugin::stop()
{
    // TuxAPE unloads the library next: nothing of it may be left running,
    // nor its translator installed.
    qApp->removeEventFilter(this);
    // The window goes, and with it a compression under way and the
    // loader's window if it is open.
    delete window_;
    QCoreApplication::removeTranslator(&translator_);
    // The entry and the button are TuxAPE's: it takes them back.
    action_ = nullptr;
    host_ = nullptr;
}

bool TuxCmpPlugin::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == qApp && event->type() == QEvent::ApplicationPaletteChange && host_ && action_)
        host_->addButton(action_, compressIcon());
    return false;
}

void TuxCmpPlugin::showWindow()
{
    if (!window_)
        window_ = new CompressorWindow(host_, host_->window());
    window_->show();
    window_->raise();
    window_->activateWindow();
    window_->refreshDiscs();
}

}  // namespace tuxcmp
