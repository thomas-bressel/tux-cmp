#include "tuxcmpplugin.h"

#include <QAction>
#include <QApplication>
#include <QEvent>

#include "compressorwindow.h"
#include "icons.h"

bool TuxCmpPlugin::start(tuxape::PluginHost* host, QString*)
{
    host_ = host;
    action_ = host->addAction(tr("TuxCMP – ZX0 Com&pressor..."), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F8));
    host->addButton(action_, compressIcon());
    connect(action_, &QAction::triggered, this, &TuxCmpPlugin::showWindow);
    qApp->installEventFilter(this);
    return true;
}

void TuxCmpPlugin::stop()
{
    qApp->removeEventFilter(this);
    // TuxAPE is unloading the library: the window goes, and a compression
    // under way with it.
    delete window_;
    // The entry and the button are TuxAPE's: it takes them back.
    action_ = nullptr;
    host_ = nullptr;
}

bool TuxCmpPlugin::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == qApp && event->type() == QEvent::ApplicationPaletteChange && host_)
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
