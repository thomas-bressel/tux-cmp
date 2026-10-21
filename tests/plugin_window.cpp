// The plugin as TuxAPE finds it: the library loaded with QPluginLoader and
// taken only if it is a tuxape::Plugin, started with a host that only notes
// what it is asked, its entry chosen and its window shown, a file compressed
// there and put on the disc in drive A:, and the plugin stopped while it
// compresses another, then unloaded. Then once more in French, as TuxAPE
// starts it when its windows are in French, and in a language TuxCMP has no
// translation for. Runs without a display (QT_QPA_PLATFORM=offscreen).
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
        if (widget->objectName() == "CompressorWindow")
            return qobject_cast<QDialog*>(widget);
    return nullptr;
}

// The next box that asks is answered No.
void answerNo()
{
    QTimer::singleShot(0, [] {
        auto* box = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        CHECK(box != nullptr);
        if (box)
            box->reject();
    });
}

// One load of the plugin, in the language the host speaks: the texts of its
// entry, its window and its button, as that language has them. In English,
// a file is also compressed and put on a disc, and saved as `picture` if
// that names a file.
struct Run {
    QString entry;
    QString title;
    QString compress;
    QString tuxDsk;
};

Run runOnce(const QString& language, const QString& folder, const QString& picture)
{
    Run run;
    QPluginLoader loader(QStringLiteral(TUXCMP_PLUGIN));
    QObject* instance = loader.instance();
    CHECK(instance != nullptr);
    if (!instance) {
        std::printf("  %s\n", qPrintable(loader.errorString()));
        return run;
    }
    // The interface TuxAPE asks of a plugin.
    CHECK(loader.metaData().value("IID").toString() == QStringLiteral("org.tuxape.Plugin/1"));
    auto* plugin = qobject_cast<tuxape::Plugin*>(instance);
    CHECK(plugin != nullptr);
    if (!plugin)
        return run;

    {
        Host host(folder);
        host.speaks = language;
        host.drive[0].present = true;
        host.drive[0].name = "work.dsk";
        QString error;
        CHECK(plugin->start(&host, &error) && error.isEmpty());
        // One entry, with its key and its button; no window until it is chosen.
        CHECK_EQ(host.actions.size(), 1);
        CHECK(host.actions.size() == 1 && host.actions[0]->shortcut() == QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F8));
        CHECK(host.buttons == 1 && host.pictures == 1);
        CHECK(compressorWindow() == nullptr);
        // The look changed: the picture again.
        QEvent look(QEvent::ApplicationPaletteChange);
        QApplication::sendEvent(qApp, &look);
        CHECK_EQ(host.pictures, 2);
        if (host.actions.isEmpty())
            return run;
        run.entry = host.actions[0]->text();

        host.actions[0]->trigger();
        QDialog* window = compressorWindow();
        CHECK(window && window->isVisible() && window->parentWidget() == &host.frame);
        host.actions[0]->trigger();
        CHECK(compressorWindow() == window);
        if (!window)
            return run;
        run.title = window->windowTitle();
        auto* compress = window->findChild<QPushButton*>("bCompress");
        auto* putTuxDsk = window->findChild<QPushButton*>("bPutTuxDsk");
        auto* tuxDskInfo = window->findChild<QLabel*>("lTuxDsk");
        auto* putDrive = window->findChild<QPushButton*>("bPutDrive");
        CHECK(compress && putTuxDsk && tuxDskInfo && putDrive);
        if (!compress || !putTuxDsk || !tuxDskInfo || !putDrive)
            return run;
        run.compress = compress->text();
        // TuxDSK is not there: its button waits for it.
        CHECK(!putTuxDsk->isEnabled());
        run.tuxDsk = tuxDskInfo->text();

        if (language == "en") {
            // A picture dropped on the window, and compressed.
            QMimeData mime;
            mime.setUrls({QUrl::fromLocalFile(QStringLiteral(TUXCMP_GOLDEN_DIR) + "/SHOP.SCR")});
            QDragEnterEvent enter(QPoint(10, 10), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(window, &enter);
            CHECK(enter.isAccepted());
            QDropEvent drop(QPointF(10, 10), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(window, &drop);
            CHECK(QTest::qWaitFor([&] { return putDrive->isEnabled(); }, 30000));
            auto* name = window->findChild<QLineEdit*>("edName");
            CHECK(name && name->text() == "SHOP.ZX0");
            if (!picture.isEmpty()) {
                QTest::qWait(50);
                window->grab().save(picture);
            }
            // Put on the disc in A:; no program made for it.
            answerNo();
            putDrive->click();
            CHECK(host.drive[0].order == QStringList{"SHOP.ZX0"});
            QFile made(QStringLiteral(TUXCMP_GOLDEN_DIR) + "/SHOP.SCR.ZX0");
            CHECK(made.open(QIODevice::ReadOnly));
            CHECK(host.drive[0].files.value("SHOP.ZX0").mid(128) == made.readAll());
            // What it remembers is in the folder TuxAPE gives it.
            CHECK(QFile::exists(QDir(folder).filePath("TuxCMP.ini")));

            // Stopped while it compresses: the window goes, and the work
            // with it.
            compress->click();
            CHECK(compress->text() != run.compress);
        }
        plugin->stop();
        CHECK(compressorWindow() == nullptr);
    }
    // Its translation is gone with it.
    CHECK(QCoreApplication::translate("tuxcmp::CompressorWindow", "&Compress") == "&Compress");
    // What the plugin left to be deleted goes before its library does, as
    // TuxAPE sees to it.
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    CHECK(loader.unload());
    return run;
}

}  // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    const QString prefix = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString();
    QTemporaryDir folder;

    // In English, the language of the sources.
    const Run english = runOnce("en", folder.path(), prefix.isEmpty() ? QString() : prefix + "tuxcmp_plugin.png");
    CHECK(english.entry == QString::fromUtf8("TuxCMP – ZX0 Com&pressor..."));
    CHECK(english.title == QString::fromUtf8("TuxCMP – ZX0 Compressor"));
    CHECK(english.compress == "&Compress");
    CHECK(english.tuxDsk.startsWith("not loaded"));

    // In French: TuxAPE's language, TuxCMP's own translation.
    const Run french = runOnce("fr", folder.path(), QString());
    CHECK(french.entry == QString::fromUtf8("TuxCMP – Com&presseur ZX0..."));
    CHECK(french.title == QString::fromUtf8("TuxCMP – Compresseur ZX0"));
    CHECK(french.compress == "&Compresser");
    CHECK(french.tuxDsk.startsWith(QString::fromUtf8("non chargé")));

    // A language TuxCMP has no translation for: English.
    const Run other = runOnce("xx", folder.path(), QString());
    CHECK(other.entry == english.entry && other.compress == english.compress);
    return checkSummary("plugin_window");
}
