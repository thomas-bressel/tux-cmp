// The plugin as TuxAPE finds it: the library loaded, started with a host
// that only notes what it is asked, its entry chosen and its window shown,
// a file compressed there and put on the disc in drive A:, and the plugin
// stopped while it compresses another. Runs without a display
// (QT_QPA_PLATFORM=offscreen).
//
//   plugin_window [prefix]   also saves a picture of the window as
//                            <prefix>tuxcmp_plugin.png

#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QPluginLoader>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QUrl>

#include "check.h"
#include "host.h"

namespace {

QDialog* compressorWindow()
{
    for (QWidget* widget : QApplication::topLevelWidgets())
        if (widget->inherits("CompressorWindow"))
            return qobject_cast<QDialog*>(widget);
    return nullptr;
}

// The next box that asks is answered: Yes or No.
void answer(bool yes)
{
    QTimer::singleShot(0, [yes] {
        auto* box = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        CHECK(box != nullptr);
        if (!box)
            return;
        for (QPushButton* button : box->findChildren<QPushButton*>())
            if (button->text().remove('&') == (yes ? "Yes" : "No")) {
                button->click();
                return;
            }
        box->reject();
    });
}

}  // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    const QString prefix = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString();
    QTemporaryDir scratch;

    QPluginLoader loader(QStringLiteral(TUXCMP_PLUGIN));
    QObject* instance = loader.instance();
    CHECK(instance != nullptr);
    if (!instance) {
        std::printf("  %s\n", qPrintable(loader.errorString()));
        return checkSummary("plugin_window");
    }
    auto* plugin = qobject_cast<tuxape::Plugin*>(instance);
    CHECK(plugin != nullptr);
    if (!plugin)
        return checkSummary("plugin_window");

    {
        Host host(scratch.path());
        host.drive[0].present = true;
        host.drive[0].name = "work.dsk";
        QString error;
        CHECK(plugin->start(&host, &error) && error.isEmpty());
        // One entry, with its key and its button; no window until it is chosen.
        CHECK_EQ(host.actions.size(), 1);
        CHECK(host.actions.size() == 1 && host.actions[0]->text() == "TuxCMP – ZX0 Com&pressor...");
        CHECK(host.actions.size() == 1 && host.actions[0]->shortcut() == QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F8));
        CHECK(host.buttons == 1 && host.pictures == 1);
        CHECK(compressorWindow() == nullptr);
        QEvent look(QEvent::ApplicationPaletteChange);
        QApplication::sendEvent(&app, &look);
        CHECK_EQ(host.pictures, 2);
        if (host.actions.isEmpty())
            return checkSummary("plugin_window");

        host.actions[0]->trigger();
        QDialog* window = compressorWindow();
        CHECK(window && window->isVisible() && window->parentWidget() == &host.frame);
        CHECK(window && window->windowTitle() == "TuxCMP – ZX0 Compressor");
        host.actions[0]->trigger();
        CHECK(compressorWindow() == window);
        if (!window)
            return checkSummary("plugin_window");

        // TuxDSK is not there: its button waits for it.
        auto* putTuxDsk = window->findChild<QPushButton*>("bPutTuxDsk");
        auto* tuxDskInfo = window->findChild<QLabel*>("lTuxDsk");
        CHECK(putTuxDsk && !putTuxDsk->isEnabled());
        CHECK(tuxDskInfo && tuxDskInfo->text().startsWith("not loaded"));

        // A picture dropped on the window, and compressed.
        QMimeData mime;
        mime.setUrls({QUrl::fromLocalFile(QStringLiteral(TUXCMP_GOLDEN_DIR) + "/SHOP.SCR")});
        QDragEnterEvent enter(QPoint(10, 10), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(window, &enter);
        CHECK(enter.isAccepted());
        QDropEvent drop(QPointF(10, 10), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(window, &drop);
        auto* putDrive = window->findChild<QPushButton*>("bPutDrive");
        CHECK(putDrive != nullptr);
        if (!putDrive)
            return checkSummary("plugin_window");
        CHECK(QTest::qWaitFor([&] { return putDrive->isEnabled(); }, 30000));
        auto* name = window->findChild<QLineEdit*>("eName");
        CHECK(name && name->text() == "SHOP.ZX0");
        if (!prefix.isEmpty()) {
            QTest::qWait(50);
            window->grab().save(prefix + "tuxcmp_plugin.png");
        }
        // Put on the disc in A:; no program made for it.
        answer(false);
        putDrive->click();
        CHECK(host.drive[0].order == QStringList{"SHOP.ZX0"});
        QFile made(QStringLiteral(TUXCMP_GOLDEN_DIR) + "/SHOP.SCR.ZX0");
        CHECK(made.open(QIODevice::ReadOnly));
        CHECK(host.drive[0].files.value("SHOP.ZX0").mid(128) == made.readAll());
        CHECK(QFile::exists(QDir(scratch.path()).filePath("TuxCMP.ini")));

        // Stopped while it compresses: the window goes, and the work with it.
        auto* compress = window->findChild<QPushButton*>("bCompress");
        if (compress)
            compress->click();
        CHECK(compress && compress->text() == "&Stop");
        plugin->stop();
        CHECK(compressorWindow() == nullptr);
    }
    // What the plugin left to be deleted goes before its library does, as
    // TuxAPE sees to it.
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    CHECK(loader.unload());
    return checkSummary("plugin_window");
}
