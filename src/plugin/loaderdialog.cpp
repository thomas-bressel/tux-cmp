#include "loaderdialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "addresses.h"
#include "cpc/depacker.h"
#include "cpc/amsdos.h"
#include "plugin.h"
#include "prompts.h"

namespace tuxcmp {

using cpc::Finding;
using cpc::Problem;

namespace {

QString count(int bytes)
{
    return LoaderDialog::tr("%Ln byte(s)", nullptr, bytes);
}

// A line of what is found wrong: said in words, not by a colour alone.
QString findingLine(bool fatal, const QString& text)
{
    return fatal ? QStringLiteral("<b>%1</b>").arg(LoaderDialog::tr("Error: %1").arg(text).toHtmlEscaped())
                 : LoaderDialog::tr("Warning: %1").arg(text).toHtmlEscaped();
}

QLineEdit* addressEdit(const char* name)
{
    auto* edit = new QLineEdit;
    edit->setObjectName(name);
    edit->setMaximumWidth(edit->fontMetrics().horizontalAdvance(QStringLiteral("&FFFFFF")) + 16);
    return edit;
}

QLineEdit* nameEdit(const char* name)
{
    auto* edit = new QLineEdit;
    edit->setObjectName(name);
    edit->setMaxLength(12);
    edit->setMaximumWidth(edit->fontMetrics().horizontalAdvance(QStringLiteral("WWWWWWWW.WWW")) + 16);
    return edit;
}

QString stem(const std::string& name)
{
    return QString::fromStdString(name.substr(0, name.find('.')));
}

}  // namespace

LoaderDialog::LoaderDialog(tuxape::PluginHost* host, const DiscTarget& target, std::vector<uint8_t> original,
                           tuxcmp::zx0::Packed packed, const tuxcmp::cpc::Loader& loader, QWidget* parent)
    : QDialog(parent)
    , host_(host)
    , target_(target)
    , original_(std::move(original))
    , packed_(std::move(packed))
    , loader_(loader)
{
    setWindowTitle(tr("TuxCMP – Decompression Program"));
    setObjectName("LoaderDialog");

    // ---- The data ----
    destination_ = addressEdit("edDestination");
    destinationInfo_ = new QLabel;
    destinationInfo_->setObjectName("lDestination");
    run_ = new QCheckBox(tr("Then &run at:"));
    run_->setObjectName("ckRun");
    run_->setToolTip(tr("Once back, the data are a program, run there; BASIC is not gone back to."));
    entry_ = addressEdit("edEntry");
    mode_ = new QComboBox;
    mode_->setObjectName("cbMode");
    mode_->addItem(tr("Unchanged"), -1);
    for (int mode = 0; mode <= 2; ++mode)
        mode_->addItem(QString::number(mode), mode);
    waitKey_ = new QCheckBox(tr("&Wait for a key at the end"));
    waitKey_->setObjectName("ckWaitKey");
    waitKey_->setToolTip(tr("For a picture to be seen before BASIC writes Ready over it."));
    auto* dataBox = new QGroupBox(tr("The Data"));
    auto* data = new QGridLayout(dataBox);
    auto* destinationLabel = new QLabel(tr("Decompressed &to:"));
    destinationLabel->setBuddy(destination_);
    data->addWidget(destinationLabel, 0, 0);
    data->addWidget(destination_, 0, 1);
    data->addWidget(destinationInfo_, 0, 2);
    data->addWidget(run_, 1, 0);
    data->addWidget(entry_, 1, 1);
    auto* modeLabel = new QLabel(tr("Screen &mode first:"));
    modeLabel->setBuddy(mode_);
    data->addWidget(modeLabel, 2, 0);
    data->addWidget(mode_, 2, 1);
    data->addWidget(waitKey_, 3, 0, 1, 3);
    data->setColumnStretch(2, 1);

    // ---- The files ----
    basicName_ = nameEdit("edBasicName");
    programName_ = nameEdit("edProgramName");
    program_ = addressEdit("edProgram");
    programInfo_ = new QLabel;
    programInfo_->setObjectName("lProgram");
    packedName_ = new QLabel;
    packedName_->setObjectName("lPackedName");
    source_ = addressEdit("edSource");
    packedInfo_ = new QLabel;
    packedInfo_->setObjectName("lPacked");
    auto* place = new QPushButton(tr("&Place Automatically"));
    place->setObjectName("bPlace");
    place->setAutoDefault(false);
    place->setToolTip(tr("The compressed file and the program put where they lie over nothing, as high as they can."));
    connect(place, &QPushButton::clicked, this, &LoaderDialog::placeAutomatically);
    auto* filesBox = new QGroupBox(tr("The Files"));
    auto* files = new QGridLayout(filesBox);
    auto* basicLabel = new QLabel(tr("&BASIC loader:"));
    basicLabel->setBuddy(basicName_);
    files->addWidget(basicLabel, 0, 0);
    files->addWidget(basicName_, 0, 1);
    auto* programLabel = new QLabel(tr("Pro&gram:"));
    programLabel->setBuddy(programName_);
    files->addWidget(programLabel, 1, 0);
    files->addWidget(programName_, 1, 1);
    files->addWidget(new QLabel(tr("at")), 1, 2);
    files->addWidget(program_, 1, 3);
    files->addWidget(programInfo_, 1, 4);
    files->addWidget(new QLabel(tr("Compressed file:")), 2, 0);
    files->addWidget(packedName_, 2, 1);
    auto* sourceLabel = new QLabel(tr("&at"));
    sourceLabel->setBuddy(source_);
    files->addWidget(sourceLabel, 2, 2);
    files->addWidget(source_, 2, 3);
    files->addWidget(packedInfo_, 2, 4);
    files->addWidget(place, 3, 0, 1, 2, Qt::AlignLeft);
    files->setColumnStretch(4, 1);

    // ---- The loader ----
    listing_ = new QPlainTextEdit;
    listing_->setObjectName("teListing");
    listing_->setReadOnly(true);
    listing_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    listing_->setFixedHeight(listing_->fontMetrics().lineSpacing() * 8 + 12);
    listing_->setLineWrapMode(QPlainTextEdit::NoWrap);
    findings_ = new QLabel;
    findings_->setObjectName("lFindings");
    findings_->setWordWrap(true);
    findings_->setTextFormat(Qt::RichText);
    runAfter_ = new QCheckBox;
    runAfter_->setObjectName("ckRunAfter");
    runAfter_->setVisible(target_.kind == DiscTarget::Drive);
    auto* listingBox = new QGroupBox(tr("The Loader"));
    auto* listingLayout = new QVBoxLayout(listingBox);
    listingLayout->addWidget(listing_);
    listingLayout->addWidget(findings_);

    auto* assembler = new QPushButton(tr("Show in the Ass&embler"));
    assembler->setObjectName("bAssembler");
    assembler->setAutoDefault(false);
    assembler->setEnabled(host_ != nullptr);
    assembler->setToolTip(tr("The program's source, in TuxAPE's assembler."));
    connect(assembler, &QPushButton::clicked, this, &LoaderDialog::showInAssembler);
    auto* saveSource = new QPushButton(tr("Save the &Source..."));
    saveSource->setObjectName("bSaveSource");
    saveSource->setAutoDefault(false);
    connect(saveSource, &QPushButton::clicked, this, &LoaderDialog::chooseSaveSource);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    ok_ = buttons->button(QDialogButtonBox::Ok);
    ok_->setText(tr("Put on the &Disc"));
    connect(buttons, &QDialogButtonBox::accepted, this, &LoaderDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto* bottom = new QHBoxLayout;
    bottom->addWidget(assembler);
    bottom->addWidget(saveSource);
    bottom->addStretch(1);
    bottom->addWidget(buttons);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("For %1, on %2.").arg(QString::fromStdString(loader_.packedName), discs::describe(target_))));
    layout->addWidget(dataBox);
    layout->addWidget(filesBox);
    layout->addWidget(listingBox);
    layout->addWidget(runAfter_);
    layout->addLayout(bottom);

    showLoader();
    for (QLineEdit* edit : {destination_, entry_, basicName_, programName_, program_, source_})
        connect(edit, &QLineEdit::textEdited, this, &LoaderDialog::update);
    for (QCheckBox* box : {run_, waitKey_})
        connect(box, &QCheckBox::toggled, this, &LoaderDialog::update);
    connect(mode_, &QComboBox::activated, this, &LoaderDialog::update);
    update();
}

tuxcmp::cpc::Loader LoaderDialog::loader() const
{
    tuxcmp::cpc::Loader loader = loader_;
    loader.basicName = basicName_->text().trimmed().toUpper().toStdString();
    loader.programName = programName_->text().trimmed().toUpper().toStdString();
    loader.destination = readAddress(destination_->text()).value_or(loader.destination);
    loader.program = readAddress(program_->text()).value_or(loader.program);
    loader.source = readAddress(source_->text()).value_or(loader.source);
    loader.entry = std::nullopt;
    if (run_->isChecked())
        loader.entry = readAddress(entry_->text()).value_or(loader_.entry.value_or(loader.destination));
    loader.mode = mode_->currentData().toInt();
    loader.waitKey = waitKey_->isChecked();
    return loader;
}

void LoaderDialog::setLoader(const tuxcmp::cpc::Loader& loader)
{
    loader_ = loader;
    showLoader();
    update();
}

void LoaderDialog::placeAutomatically()
{
    loader_ = loader();
    tuxcmp::cpc::placeLoader(loader_, static_cast<int>(original_.size()), packed_);
    showLoader();
    update();
}

std::vector<Finding> LoaderDialog::findings() const
{
    return tuxcmp::cpc::checkLoader(loader(), original_, packed_.data);
}

bool LoaderDialog::addressesRead() const
{
    for (const QLineEdit* edit : {destination_, program_, source_})
        if (!readAddress(edit->text()))
            return false;
    return !run_->isChecked() || readAddress(entry_->text());
}

QString LoaderDialog::listing() const
{
    return QString::fromStdString(tuxcmp::cpc::loaderListing(loader()));
}

bool LoaderDialog::runAfter() const
{
    return target_.kind == DiscTarget::Drive && runAfter_->isChecked();
}

void LoaderDialog::setRunAfter(bool on)
{
    runAfter_->setChecked(on);
}

QString LoaderDialog::source() const
{
    const tuxcmp::cpc::Loader made = loader();
    return QString::fromStdString(
        tuxcmp::cpc::depackerSource({made.program, made.source, made.destination, made.entry}, made.packedName));
}

bool LoaderDialog::saveSource(const QString& path)
{
    QFile file(path);
    const QByteArray text = source().toUtf8();
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(text) != text.size()) {
        prompts::problem(this, tr("%1 cannot be written: %2").arg(QDir::toNativeSeparators(path), file.errorString()));
        return false;
    }
    return true;
}

void LoaderDialog::showInAssembler()
{
    if (!host_)
        return;
    const QString path = QDir(host_->dataFolder()).filePath(stem(loader().programName) + ".asm");
    if (saveSource(path))
        host_->openInAssembler(path);
}

void LoaderDialog::chooseSaveSource()
{
    const QString folder = host_ ? host_->dataFolder() : QDir::homePath();
    const QString path = QFileDialog::getSaveFileName(this, tr("Save the Program's Source"),
                                                      QDir(folder).filePath(stem(loader().programName) + ".asm"),
                                                      tr("Assembler sources (*.asm);;All files (*)"));
    if (!path.isEmpty())
        saveSource(path);
}

bool LoaderDialog::putFiles()
{
    const tuxcmp::cpc::Loader made = loader();
    const auto files = tuxcmp::cpc::makeLoaderFiles(made, packed_.data);
    if (!files) {
        prompts::problem(this, tr("The loader cannot be made."));
        return false;
    }
    const auto bytes = [](const std::vector<uint8_t>& file) {
        return QByteArray(reinterpret_cast<const char*>(file.data()), static_cast<qsizetype>(file.size()));
    };
    // The compressed file is TuxCMP's own, put there a moment ago: it is
    // replaced without a question, with the header the loader loads it by.
    if (!discs::put(host_, target_, QString::fromStdString(made.packedName), bytes(files->packed), true, this)
        || !discs::put(host_, target_, QString::fromStdString(made.programName), bytes(files->program), false, this)
        || !discs::put(host_, target_, QString::fromStdString(made.basicName), bytes(files->basic), false, this))
        return false;
    if (runAfter() && host_ && target_.kind == DiscTarget::Drive) {
        QString typed;
        if (target_.drive != 0)
            typed += QStringLiteral("|%1\n").arg(discs::driveLetter(target_.drive));
        typed += QStringLiteral("RUN\"%1\"\n").arg(QString::fromStdString(made.basicName));
        host_->autoType(typed);
    }
    return true;
}

void LoaderDialog::accept()
{
    update();
    if (!ok_->isEnabled())
        return;
    if (putFiles())
        QDialog::accept();
}

void LoaderDialog::showLoader()
{
    destination_->setText(addressText(loader_.destination));
    run_->setChecked(loader_.entry.has_value());
    entry_->setText(addressText(loader_.entry.value_or(loader_.destination)));
    mode_->setCurrentIndex(std::max(0, mode_->findData(loader_.mode)));
    waitKey_->setChecked(loader_.waitKey);
    basicName_->setText(QString::fromStdString(loader_.basicName));
    programName_->setText(QString::fromStdString(loader_.programName));
    program_->setText(addressText(loader_.program));
    packedName_->setText(QString::fromStdString(loader_.packedName));
    source_->setText(addressText(loader_.source));
}

void LoaderDialog::update()
{
    const tuxcmp::cpc::Loader made = loader();
    const int length = static_cast<int>(original_.size());
    const int programLength = tuxcmp::cpc::depackerLength(made.entry.has_value());
    const int packedLength = static_cast<int>(packed_.data.size());
    destinationInfo_->setText(tr("%1, to %2").arg(count(length), addressText(made.destination + length - 1)));
    programInfo_->setText(tr("%1, to %2").arg(count(programLength), addressText(made.program + programLength - 1)));
    packedInfo_->setText(tr("%1, to %2").arg(count(packedLength), addressText(made.source + packedLength - 1)));
    entry_->setEnabled(run_->isChecked());
    // Run at once, the data never come back to BASIC.
    waitKey_->setEnabled(!run_->isChecked());
    runAfter_->setText(tr("R&un it once it is on the disc: RUN\"%1\" typed on the CPC's keyboard")
                           .arg(QString::fromStdString(made.basicName)));
    listing_->setPlainText(QString::fromStdString(tuxcmp::cpc::loaderListing(made)));

    QStringList lines;
    bool fine = addressesRead();
    if (!fine)
        lines << findingLine(true, tr("An address is not one: four hexadecimal digits, as &C000."));
    const std::vector<Finding> found = findings();
    for (const Finding& finding : found)
        lines << findingLine(finding.fatal, describe(finding));
    fine = fine && tuxcmp::cpc::works(found);
    if (lines.isEmpty())
        lines << tr("Tried in the CPC's memory: the data come back whole.").toHtmlEscaped();
    findings_->setText(lines.join(QStringLiteral("<br>")));
    ok_->setEnabled(fine);
}

QString LoaderDialog::describe(const Finding& finding)
{
    switch (finding.problem) {
    case Problem::BadName:
        return tr("A name is not one a file can have on a disc: eight characters at most, a dot and three.");
    case Problem::SameNames:
        return tr("Two of the files have the same name.");
    case Problem::DestinationPastEnd:
        return tr("The data would run past &FFFF.");
    case Problem::DestinationTooLow:
        return tr("The data would go below &0040, over the restarts the machine needs.");
    case Problem::DestinationOnSystem:
        return tr("The data would go over AMSDOS, the firmware and the stack, from &A67C to &BFFF.");
    case Problem::DestinationOnBasic:
        return finding.fatal ? tr("The data would go over the BASIC loader, below &0400: only a program run at once can go there.")
                             : tr("The data go over the BASIC loader, below &0400: the program they are is run at once.");
    case Problem::DestinationUnkept:
        return tr("Under &1300, MEMORY cannot keep the data from BASIC: they stay as they are until BASIC needs the room.");
    case Problem::SourceOutside:
        return tr("The compressed file cannot be loaded there: it must lie from &1300 to &A67B, or in the screen from &C000; "
                  "under &1300, BASIC has not the room it needs to load it.");
    case Problem::ProgramOutside:
        return tr("The program cannot be loaded there: it must lie from &1300 to &A67B, or in the screen from &C000; "
                  "under &1300, BASIC has not the room it needs to load it.");
    case Problem::ProgramOverwritten:
        return tr("The program would lie over the data or over the compressed file.");
    case Problem::SourceOverwritten:
        return tr("Decompressed there, the data would write over the compressed file before it is read: move one or the "
                  "other, or place them automatically.");
    case Problem::EntryOutside:
        return tr("The address the data are run at is not among them.");
    case Problem::ScreenWithoutMode:
        return tr("Without MODE, a picture may show shifted if the screen has scrolled.");
    }
    return {};
}

}  // namespace tuxcmp
