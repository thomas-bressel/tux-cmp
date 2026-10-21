#pragma once

#include <QObject>
#include <QPointer>
#include <QTranslator>

#include "plugin.h"

namespace tuxcmp {

class CompressorWindow;

// What TuxAPE loads: it puts TuxCMP's entry in the Plugins menu and its
// button on the control panel, and shows the compressor's window when
// either is chosen.
class TuxCmpPlugin : public QObject, public tuxape::Plugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.tuxape.Plugin/1")
    Q_INTERFACES(tuxape::Plugin)

public:
    bool start(tuxape::PluginHost* host, QString* error) override;
    void stop() override;

protected:
    // The application's look has changed: the button's picture is drawn
    // again for it.
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    tuxape::PluginHost* host_ = nullptr;
    QAction* action_ = nullptr;
    QPointer<CompressorWindow> window_;
    QTranslator translator_;

    void showWindow();
};

}  // namespace tuxcmp
