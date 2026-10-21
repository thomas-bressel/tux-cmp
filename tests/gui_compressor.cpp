// The compressor's window and the loader's, driven as a user drives them,
// with a TuxAPE and a TuxDSK of the test's own to take the files: a picture
// compressed and put on the disc in drive A:, a loader made for it there,
// the file put again and the questions that come with it, a drive without a
// disc, TuxDSK there, too old, and gone; a file with an AMSDOS header; a
// compression stopped; the loader's window told where its parts go. Runs
// without a display (QT_QPA_PLATFORM=offscreen).
//
//   gui_compressor [prefix]   also saves pictures of the windows as
//                             <prefix>compressor.png and <prefix>loader.png

#include <deque>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

#include "check.h"
#include "compressorwindow.h"
#include "cpc/amsdos.h"
#include "cpc/basic.h"
#include "golden.h"
#include "host.h"
#include "loaderdialog.h"
#include "prompts.h"

namespace {

// A TuxDSK of the test's own: what it offers other plugins, and a note of
// what it was given.
class TuxDsk : public QObject {
    Q_OBJECT

public:
    QMap<QString, QByteArray> files;
    QList<bool> replaced;

    Q_INVOKABLE bool putFile(const QString& name, const QByteArray& content, bool replace)
    {
        files[name] = content;
        replaced << replace;
        return true;
    }
    Q_INVOKABLE QString discName() const { return files.isEmpty() ? QString() : QStringLiteral("game.dsk"); }
};

// A TuxDSK from before it offered anything.
class OldTuxDsk : public QObject {
    Q_OBJECT
};

QStringList g_told;
QStringList g_asked;
std::deque<bool> g_answers;

void answerBoxes()
{
    prompts::setProblemHandler([](const QString& text) { g_told << text; });
    prompts::setAnswerer([](const QString& text) {
        g_asked << text;
        if (g_answers.empty()) {
            std::printf("a question nothing was planned for: %s\n", qPrintable(text));
            ++g_failures;
            return false;
        }
        const bool answer = g_answers.front();
        g_answers.pop_front();
        return answer;
    });
}

// The loader's window, when it is shown, is given to `fill`.
void whenLoader(std::function<void(LoaderDialog*)> fill)
{
    QTimer::singleShot(0, [fill = std::move(fill)] {
        auto* dialog = qobject_cast<LoaderDialog*>(QApplication::activeModalWidget());
        CHECK(dialog != nullptr);
        if (dialog)
            fill(dialog);
    });
}

QByteArray bytes(const std::vector<uint8_t>& data)
{
    return QByteArray(reinterpret_cast<const char*>(data.data()), static_cast<qsizetype>(data.size()));
}

std::vector<uint8_t> vector(const QByteArray& data)
{
    return std::vector<uint8_t>(data.begin(), data.end());
}

bool waitFor(CompressorWindow& window)
{
    return QTest::qWaitFor([&] { return !window.compressing(); }, 60000);
}

}  // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    // Numbers as the checks write them.
    QLocale::setDefault(QLocale::c());
    const QString prefix = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString();
    QTemporaryDir scratch;
    answerBoxes();

    Host host(scratch.path());
    host.drive[0].present = true;
    host.drive[0].name = "work.dsk";
    CompressorWindow window(&host);
    window.show();
    auto* putDrive = window.findChild<QPushButton*>("bPutDrive");
    auto* putTuxDsk = window.findChild<QPushButton*>("bPutTuxDsk");
    auto* tuxDskInfo = window.findChild<QLabel*>("lTuxDsk");
    auto* drives = window.findChild<QComboBox*>("cbDrive");
    auto* result = window.findChild<QLabel*>("lResult");
    CHECK(putDrive && putTuxDsk && tuxDskInfo && drives && result);
    if (!putDrive || !putTuxDsk || !tuxDskInfo || !drives || !result)
        return checkSummary("gui_compressor");

    // Nothing to put anywhere yet; the drives as TuxAPE has them.
    CHECK(!putDrive->isEnabled() && !putTuxDsk->isEnabled());
    CHECK_EQ(drives->count(), 2);
    CHECK(drives->itemText(0) == "Drive A: work.dsk, 0 file(s), 178K free" && drives->itemText(1) == "Drive B: no disc");
    CHECK(tuxDskInfo->text().startsWith("not loaded"));

    // ---- A picture, to the disc in A:, with its loader ----
    const std::vector<uint8_t> shop = golden::read("SHOP.SCR");
    const std::vector<uint8_t> shopPacked = golden::read("SHOP.SCR.ZX0");
    CHECK(window.openFile(QString::fromStdString(golden::path("SHOP.SCR"))));
    CHECK(window.compressing());
    CHECK(waitFor(window));
    CHECK(window.packed() && window.packed()->data == shopPacked);
    CHECK(result->text().contains("38.1") && result->text().contains("they are the file"));
    CHECK(window.packedName() == "SHOP.ZX0" && window.withHeader() && !window.fileHeader());
    CHECK(window.destination() == 0xC000 && !window.entry());
    const tuxcmp::cpc::Loader suggested =
        tuxcmp::cpc::suggestLoader("SHOP.ZX0", 0xC000, std::nullopt, 16384, *window.packed());
    CHECK_EQ(window.loadAddress(), suggested.source);
    CHECK(putDrive->isEnabled() && !putTuxDsk->isEnabled());
    if (!prefix.isEmpty()) {
        QTest::qWait(50);
        window.grab().save(prefix + "compressor.png");
    }

    g_answers = {true};
    whenLoader([&](LoaderDialog* dialog) {
        CHECK(dialog->windowTitle() == "Decompression Program");
        const tuxcmp::cpc::Loader loader = dialog->loader();
        CHECK(loader.destination == 0xC000 && loader.source == suggested.source && loader.program == suggested.program);
        CHECK(loader.mode == 1 && loader.waitKey && !loader.entry);
        CHECK(dialog->findings().empty());
        auto* listing = dialog->findChild<QPlainTextEdit*>("tListing");
        CHECK(listing && listing->toPlainText() == QString::fromStdString(tuxcmp::cpc::loaderListing(loader)));
        auto* findings = dialog->findChild<QLabel*>("lFindings");
        CHECK(findings && findings->text().contains("come back whole"));
        CHECK(dialog->findChild<QCheckBox*>("ckRunAfter") && dialog->findChild<QCheckBox*>("ckRunAfter")->isVisibleTo(dialog));
        dialog->setRunAfter(true);
        if (!prefix.isEmpty()) {
            QTest::qWait(50);
            dialog->grab().save(prefix + "loader.png");
        }
        dialog->accept();
    });
    putDrive->click();
    CHECK(g_asked.size() == 1 && g_asked.last().contains("Do you want a program made to decompress it?"));
    CHECK(g_told.isEmpty());
    // The three files, the compressed one first, as the loader has them.
    CHECK(host.drive[0].order == QStringList({"SHOP.ZX0", "SHOP.BIN", "SHOP.BAS"}));
    const auto files = tuxcmp::cpc::makeLoaderFiles(suggested, shopPacked);
    CHECK(files.has_value());
    if (files) {
        CHECK(host.drive[0].files.value("SHOP.ZX0") == bytes(files->packed));
        CHECK(host.drive[0].files.value("SHOP.BIN") == bytes(files->program));
        CHECK(host.drive[0].files.value("SHOP.BAS") == bytes(files->basic));
    }
    CHECK(host.typed == QStringList{"RUN\"SHOP.BAS\"\n"});
    CHECK(window.status() == "SHOP.BAS, SHOP.BIN and SHOP.ZX0 are on the disc in drive A:; RUN\"SHOP\" runs them.");
    CHECK(drives->itemText(0) == "Drive A: work.dsk, 3 file(s), 178K free");

    // Put again: asked before it replaces the one there, then about the
    // program, which is not wanted this time.
    g_asked.clear();
    g_answers = {true, false};
    CHECK(window.putOn(DiscTarget::inDrive(0)));
    CHECK(g_asked.size() == 2 && g_asked[0] == "SHOP.ZX0 is already on the disc in drive A:. Replace it?");
    // Not replaced: nothing else is asked.
    g_asked.clear();
    g_answers = {false};
    CHECK(!window.putOn(DiscTarget::inDrive(0)));
    CHECK_EQ(g_asked.size(), 1);

    // Drive B: has no disc: its button waits for one, and the file cannot go.
    window.setDrive(1);
    CHECK(window.drive() == 1 && !putDrive->isEnabled());
    CHECK(!window.putOn(DiscTarget::inDrive(1), false));
    CHECK(g_told.size() == 1 && g_told.last().startsWith("There is no disc in drive B:"));
    host.drive[1].present = true;
    host.drive[1].writable = false;
    window.refreshDiscs();
    CHECK(drives->itemText(1) == "Drive B: a disc, which files cannot be put on" && !putDrive->isEnabled());
    window.setDrive(0);
    CHECK(putDrive->isEnabled());

    // ---- TuxDSK ----
    TuxDsk tuxDsk;
    host.tuxDsk = &tuxDsk;
    window.refreshDiscs();
    CHECK(putTuxDsk->isEnabled() && tuxDskInfo->text() == "its first disc: none yet, one is made for the file.");
    g_answers = {true};
    whenLoader([](LoaderDialog* dialog) {
        // Not for a drive: nothing to run.
        CHECK(!dialog->findChild<QCheckBox*>("ckRunAfter")->isVisibleTo(dialog));
        dialog->accept();
    });
    putTuxDsk->click();
    CHECK(tuxDsk.files.keys() == QStringList({"SHOP.BAS", "SHOP.BIN", "SHOP.ZX0"}));
    // The compressed file is put twice: the second time replacing itself.
    CHECK(tuxDsk.replaced == QList<bool>({false, true, false, false}));
    CHECK(tuxDsk.files.value("SHOP.BAS").mid(128) == bytes(*tuxcmp::cpc::tokenize(tuxcmp::cpc::loaderListing(suggested))));
    window.refreshDiscs();
    CHECK(tuxDskInfo->text() == "its first disc, game.dsk.");
    CHECK_EQ(host.typed.size(), 1);

    OldTuxDsk old;
    host.tuxDsk = &old;
    window.refreshDiscs();
    CHECK(!putTuxDsk->isEnabled() && tuxDskInfo->text().startsWith("this version cannot"));
    g_told.clear();
    CHECK(!window.putOn(DiscTarget::tuxDsk(), false));
    CHECK(g_told.size() == 1 && g_told.last().contains("cannot take files from other plugins"));
    host.tuxDsk = nullptr;
    g_told.clear();
    CHECK(!window.putOn(DiscTarget::tuxDsk(), false));
    CHECK(g_told.size() == 1 && g_told.last().startsWith("TuxDSK is not loaded"));

    // ---- Without a header, to the computer ----
    window.setWithHeader(false);
    CHECK(vector(window.packedFile()) == shopPacked);
    const QString saved = scratch.filePath("SHOP.SCR.ZX0");
    CHECK(window.saveAs(saved));
    QFile written(saved);
    CHECK(written.open(QIODevice::ReadOnly) && vector(written.readAll()) == shopPacked);
    window.setWithHeader(true);

    // ---- A file with an AMSDOS header: a program at &4000, run there ----
    {
        std::vector<uint8_t> code;
        for (int i = 0; i < 3000; ++i)
            code.push_back(static_cast<uint8_t>(i * 13 % 7 + i / 100));
        const std::vector<uint8_t> file =
            tuxcmp::cpc::withHeader("GAME.BIN", {tuxcmp::cpc::kBinary, 0x4000, 0x4010, 0}, code);
        const QString path = scratch.filePath("game.bin");
        QFile out(path);
        CHECK(out.open(QIODevice::WriteOnly) && out.write(bytes(file)) == static_cast<qsizetype>(file.size()));
        out.close();
        CHECK(window.openFile(path));
        CHECK(waitFor(window));
        CHECK(window.fileHeader() && window.data() == code);
        CHECK(window.destination() == 0x4000 && window.entry() == 0x4010);
        CHECK(window.packedName() == "GAME.ZX0");
        CHECK(window.packed() && tuxcmp::zx0::decompress(window.packed()->data) == code);
        // With its header, the file is compressed whole, and nothing is run.
        window.findChild<QCheckBox*>("ckSkipHeader")->setChecked(false);
        CHECK(waitFor(window));
        CHECK(window.data() == file && window.packed() && tuxcmp::zx0::decompress(window.packed()->data) == file);
        CHECK(!window.entry() && window.destination() == 0x4000);
        window.findChild<QCheckBox*>("ckSkipHeader")->setChecked(true);
        CHECK(waitFor(window));

        // The loader: back at &4000 and run there, its parts high.
        g_answers = {true};
        whenLoader([](LoaderDialog* dialog) {
            const tuxcmp::cpc::Loader loader = dialog->loader();
            CHECK(loader.entry == 0x4010 && loader.mode == -1 && !loader.waitKey);
            CHECK(dialog->listing().contains("CALL &") && !dialog->listing().contains("BB18"));
            CHECK(dialog->source().contains("jp &4010"));
            dialog->showInAssembler();
            dialog->reject();
        });
        CHECK(window.putOn(DiscTarget::inDrive(0)));
        CHECK(host.assembled.size() == 1 && host.assembled[0].endsWith("GAME.asm"));
        QFile source(host.assembled.value(0));
        CHECK(source.open(QIODevice::ReadOnly) && source.readAll().contains("call DepkZX0"));
    }

    // ---- The loader's window, told where its parts go ----
    {
        const tuxcmp::zx0::Packed packed{shopPacked, 2};
        LoaderDialog dialog(&host, DiscTarget::inDrive(0), shop, packed, suggested);
        auto* ok = dialog.findChild<QPushButton*>("bPlace");
        auto* source = dialog.findChild<QLineEdit*>("eSource");
        auto* findings = dialog.findChild<QLabel*>("lFindings");
        CHECK(ok && source && findings);
        // Over the screen: written over before it is read.
        source->setText("&C000");
        emit source->textEdited(source->text());
        CHECK(dialog.loader().source == 0xC000);
        CHECK(!tuxcmp::cpc::works(dialog.findings()));
        CHECK(findings->text().contains("write over the compressed file"));
        // An address that is not one.
        source->setText("&G000");
        emit source->textEdited(source->text());
        CHECK(!dialog.addressesRead() && findings->text().contains("An address is not one"));
        // Placed again, all is well.
        dialog.placeAutomatically();
        CHECK(dialog.addressesRead() && dialog.findings().empty() && dialog.loader().source == suggested.source);
        // A picture without MODE: told, not refused.
        auto* mode = dialog.findChild<QComboBox*>("cbMode");
        mode->setCurrentIndex(0);
        emit mode->activated(0);
        CHECK(dialog.loader().mode == -1 && tuxcmp::cpc::works(dialog.findings()) && !dialog.findings().empty());
        CHECK(findings->text().contains("Without MODE"));
    }

    // ---- Stopped ----
    {
        std::vector<uint8_t> big(60000);
        for (size_t i = 0; i < big.size(); ++i)
            big[i] = static_cast<uint8_t>(i % 251 * (i / 997));
        const QString path = scratch.filePath("big.bin");
        QFile out(path);
        CHECK(out.open(QIODevice::WriteOnly) && out.write(bytes(big)) == static_cast<qsizetype>(big.size()));
        out.close();
        CHECK(window.openFile(path));
        CHECK(window.compressing());
        window.stopCompressing();
        CHECK(!window.compressing() && !window.packed());
        CHECK(result->text() == "Stopped: nothing was compressed.");
        CHECK(!putDrive->isEnabled());
        // Gone while it compresses, it stops.
        auto* other = new CompressorWindow(&host);
        CHECK(other->openFile(path) && other->compressing());
        delete other;
    }

    // Nothing to compress.
    g_told.clear();
    const QString empty = scratch.filePath("empty.bin");
    {
        QFile made(empty);
        CHECK(made.open(QIODevice::WriteOnly));
    }
    CHECK(!window.openFile(empty));
    CHECK(g_told.size() == 1 && g_told.last().contains("is empty"));
    CHECK(!window.openFile(scratch.filePath("none.bin")));
    return checkSummary("gui_compressor");
}

#include "gui_compressor.moc"
