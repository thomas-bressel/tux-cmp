#include "compressorwindow.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMimeData>
#include <QProgressBar>
#include <QPushButton>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

#include "addresses.h"
#include "cpc/loader.h"
#include "loaderdialog.h"
#include "plugin.h"
#include "prompts.h"

namespace {

QString count(qsizetype bytes)
{
    return CompressorWindow::tr("%1 bytes").arg(QLocale().toString(static_cast<qlonglong>(bytes)));
}

QString typeName(uint8_t type)
{
    switch (type) {
    case tuxcmp::cpc::kBasic:
        return CompressorWindow::tr("BASIC program");
    case tuxcmp::cpc::kProtectedBasic:
        return CompressorWindow::tr("protected BASIC program");
    case tuxcmp::cpc::kBinary:
        return CompressorWindow::tr("binary file");
    default:
        return CompressorWindow::tr("file of type %1").arg(type);
    }
}

}  // namespace

CompressorWindow::CompressorWindow(tuxape::PluginHost* host, QWidget* parent)
    : QDialog(parent)
    , host_(host)
    , preferences_(host ? host->dataFolder() : QString())
{
    setWindowTitle(tr("TuxCMP – ZX0 Compressor"));
    setAcceptDrops(true);

    // ---- The file ----
    fileEdit_ = new QLineEdit;
    fileEdit_->setObjectName("eFile");
    fileEdit_->setReadOnly(true);
    fileEdit_->setPlaceholderText(tr("Choose a file, or drop one on the window"));
    auto* browse = new QPushButton(tr("&Browse..."));
    browse->setObjectName("bBrowse");
    browse->setAutoDefault(false);
    connect(browse, &QPushButton::clicked, this, &CompressorWindow::chooseFile);
    fileInfo_ = new QLabel;
    fileInfo_->setObjectName("lFileInfo");
    fileInfo_->setWordWrap(true);
    skipHeader_ = new QCheckBox(tr("Compress the data without their AMSDOS &header"));
    skipHeader_->setObjectName("ckSkipHeader");
    skipHeader_->setChecked(preferences_.skipHeader());
    connect(skipHeader_, &QCheckBox::toggled, this, [this](bool on) {
        preferences_.setSkipHeader(on);
        // What is compressed is no longer what was.
        if (!file_.empty() && header_)
            compress();
    });
    compress_ = new QPushButton(tr("&Compress"));
    compress_->setObjectName("bCompress");
    compress_->setAutoDefault(false);
    connect(compress_, &QPushButton::clicked, this, [this] {
        if (compressing())
            stopCompressing();
        else
            compress();
    });
    progress_ = new QProgressBar;
    progress_->setObjectName("pbProgress");
    progress_->setRange(0, 1000);
    progress_->setTextVisible(false);
    progress_->setVisible(false);
    result_ = new QLabel;
    result_->setObjectName("lResult");
    result_->setWordWrap(true);

    auto* fileBox = new QGroupBox(tr("File to Compress"));
    auto* fileLayout = new QVBoxLayout(fileBox);
    auto* fileRow = new QHBoxLayout;
    fileRow->addWidget(fileEdit_, 1);
    fileRow->addWidget(browse);
    fileLayout->addLayout(fileRow);
    fileLayout->addWidget(fileInfo_);
    fileLayout->addWidget(skipHeader_);
    auto* compressRow = new QHBoxLayout;
    compressRow->addWidget(compress_);
    compressRow->addWidget(progress_, 1);
    compressRow->addStretch(0);
    fileLayout->addLayout(compressRow);
    fileLayout->addWidget(result_);

    // ---- The compressed file ----
    name_ = new QLineEdit;
    name_->setObjectName("eName");
    name_->setMaxLength(12);
    name_->setToolTip(tr("Its name on the disc: eight characters at most, a dot and three."));
    connect(name_, &QLineEdit::textChanged, this, &CompressorWindow::updateButtons);
    headerBox_ = new QCheckBox(tr("With an AMSDOS header, loading &at"));
    headerBox_->setObjectName("ckHeader");
    headerBox_->setChecked(preferences_.header());
    headerBox_->setToolTip(tr("BASIC's LOAD needs one to load the file where it says."));
    connect(headerBox_, &QCheckBox::toggled, this, [this](bool on) {
        preferences_.setHeader(on);
        updateButtons();
    });
    load_ = new QLineEdit;
    load_->setObjectName("eLoad");
    load_->setMaximumWidth(fontMetrics().horizontalAdvance(QStringLiteral("&FFFFFF")) + 16);
    connect(load_, &QLineEdit::textChanged, this, &CompressorWindow::updateButtons);
    auto* packedBox = new QGroupBox(tr("Compressed File"));
    auto* packedLayout = new QHBoxLayout(packedBox);
    auto* nameLabel = new QLabel(tr("&Name:"));
    nameLabel->setBuddy(name_);
    packedLayout->addWidget(nameLabel);
    packedLayout->addWidget(name_);
    packedLayout->addSpacing(12);
    packedLayout->addWidget(headerBox_);
    packedLayout->addWidget(load_);
    packedLayout->addStretch(1);

    // ---- Where it goes ----
    drives_ = new QComboBox;
    drives_->setObjectName("cbDrive");
    connect(drives_, &QComboBox::activated, this, [this] {
        preferences_.setDrive(drive());
        updateButtons();
    });
    putDrive_ = new QPushButton(tr("Put on &TuxAPE's Disc"));
    putDrive_->setObjectName("bPutDrive");
    putDrive_->setAutoDefault(false);
    connect(putDrive_, &QPushButton::clicked, this, [this] { putOn(DiscTarget::inDrive(drive())); });
    tuxDskInfo_ = new QLabel;
    tuxDskInfo_->setObjectName("lTuxDsk");
    tuxDskInfo_->setWordWrap(true);
    putTuxDsk_ = new QPushButton(tr("Put on Tux&DSK's Disc"));
    putTuxDsk_->setObjectName("bPutTuxDsk");
    putTuxDsk_->setAutoDefault(false);
    connect(putTuxDsk_, &QPushButton::clicked, this, [this] { putOn(DiscTarget::tuxDsk()); });
    save_ = new QPushButton(tr("&Save as File..."));
    save_->setObjectName("bSave");
    save_->setAutoDefault(false);
    connect(save_, &QPushButton::clicked, this, &CompressorWindow::chooseSave);
    auto* whereBox = new QGroupBox(tr("Put It On"));
    auto* where = new QGridLayout(whereBox);
    where->addWidget(new QLabel(tr("TuxAPE:")), 0, 0);
    where->addWidget(drives_, 0, 1);
    where->addWidget(putDrive_, 0, 2);
    where->addWidget(new QLabel(tr("TuxDSK:")), 1, 0);
    where->addWidget(tuxDskInfo_, 1, 1);
    where->addWidget(putTuxDsk_, 1, 2);
    where->addWidget(new QLabel(tr("Computer:")), 2, 0);
    where->addWidget(new QLabel(tr("a .ZX0 file, as PackZX0 makes them")), 2, 1);
    where->addWidget(save_, 2, 2);
    where->setColumnStretch(1, 1);

    status_ = new QLabel;
    status_->setObjectName("lStatus");
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
    auto* bottom = new QHBoxLayout;
    bottom->addWidget(status_, 1);
    bottom->addWidget(buttons);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(fileBox);
    layout->addWidget(packedBox);
    layout->addWidget(whereBox);
    layout->addLayout(bottom);

    progressTimer_ = new QTimer(this);
    connect(progressTimer_, &QTimer::timeout, this, &CompressorWindow::showProgress);

    refreshDiscs();
    setDrive(preferences_.drive());
    resize(560, sizeHint().height());
}

CompressorWindow::~CompressorWindow()
{
    stopCompressing();
}

bool CompressorWindow::openFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        prompts::problem(this, tr("%1 cannot be read: %2").arg(QDir::toNativeSeparators(path), file.errorString()));
        return false;
    }
    const QByteArray bytes = file.readAll();
    if (bytes.isEmpty()) {
        prompts::problem(this, tr("%1 is empty: there is nothing to compress.").arg(QFileInfo(path).fileName()));
        return false;
    }
    if (bytes.size() > tuxcmp::zx0::kMaxInput + static_cast<qsizetype>(tuxcmp::cpc::kHeaderSize)) {
        prompts::problem(this, tr("%1 is too long: TuxCMP compresses files of %2 at most.")
                                   .arg(QFileInfo(path).fileName(), count(tuxcmp::zx0::kMaxInput)));
        return false;
    }
    stopCompressing();
    path_ = path;
    file_.assign(bytes.begin(), bytes.end());
    header_ = tuxcmp::cpc::readHeader(file_);
    forget();
    preferences_.setInputFolder(QFileInfo(path).absolutePath());

    fileEdit_->setText(QDir::toNativeSeparators(path));
    QString info = count(bytes.size()) + '.';
    if (header_) {
        info += ' ' + tr("An AMSDOS header: a %1 of %2, loading at %3").arg(typeName(header_->type), count(header_->length),
                                                                             addressText(header_->loadAddress));
        if (header_->type == tuxcmp::cpc::kBinary && header_->entryAddress)
            info += tr(", run at %1").arg(addressText(header_->entryAddress));
        info += '.';
    }
    fileInfo_->setText(info);
    skipHeader_->setEnabled(header_.has_value());
    name_->setText(QString::fromStdString(tuxcmp::cpc::nameLike(QFileInfo(path).fileName().toStdString(), "ZX0")));
    setStatus(QString());
    compress();
    return true;
}

std::vector<uint8_t> CompressorWindow::data() const
{
    if (!header_ || !skipHeader_->isChecked())
        return file_;
    // What AMSDOS would load: the length the header says, not the rest of
    // the last record.
    const auto begin = file_.begin() + static_cast<std::ptrdiff_t>(tuxcmp::cpc::kHeaderSize);
    const size_t after = file_.size() - tuxcmp::cpc::kHeaderSize;
    const size_t length = header_->length > 0 && static_cast<size_t>(header_->length) < after ? static_cast<size_t>(header_->length) : after;
    return std::vector<uint8_t>(begin, begin + static_cast<std::ptrdiff_t>(length));
}

void CompressorWindow::compress()
{
    stopCompressing();
    forget();
    std::vector<uint8_t> input = data();
    if (input.empty()) {
        result_->setText(file_.empty() ? QString() : tr("There is nothing to compress after the header."));
        updateButtons();
        return;
    }
    job_ = std::make_shared<Job>();
    job_->data = std::move(input);
    job_->total = static_cast<int>(job_->data.size());
    auto job = job_;
    thread_ = QThread::create([job] {
        job->result = tuxcmp::zx0::compress(job->data, [job](int done, int total) {
            job->done = done;
            job->total = total;
            return !job->stop.load();
        });
    });
    // Told when it is over, unless it was stopped first: that one is over
    // by then.
    connect(thread_, &QThread::finished, this, [this, generation = ++generation_] {
        if (generation == generation_)
            finished();
    });
    progress_->setValue(0);
    progress_->setVisible(true);
    compress_->setText(tr("&Stop"));
    result_->setText(tr("Compressing %1...").arg(count(static_cast<qsizetype>(job_->data.size()))));
    progressTimer_->start(50);
    thread_->start();
    updateButtons();
}

void CompressorWindow::stopCompressing()
{
    if (!thread_)
        return;
    job_->stop = true;
    ++generation_;
    finished();
}

void CompressorWindow::finished()
{
    if (!thread_)
        return;
    // Over, or about to be: it is let go of here, before the plugin can be.
    thread_->wait();
    delete thread_;
    thread_ = nullptr;
    progressTimer_->stop();
    progress_->setVisible(false);
    compress_->setText(tr("&Compress"));
    const std::shared_ptr<Job> job = std::move(job_);
    if (job->stop || !job->result) {
        result_->setText(job->stop ? tr("Stopped: nothing was compressed.") : tr("The file could not be compressed."));
    } else {
        packed_ = std::move(job->result);
        // What the CPC will get back, checked before it goes anywhere.
        checked_ = tuxcmp::zx0::decompress(packed_->data, job->data.size()) == job->data;
        const double share = 100.0 * static_cast<double>(packed_->data.size()) / static_cast<double>(job->data.size());
        QString text = tr("%1: %2 % of %3.")
                           .arg(count(static_cast<qsizetype>(packed_->data.size())), QLocale().toString(share, 'f', 1),
                                count(static_cast<qsizetype>(job->data.size())));
        text += ' ' + (checked_ ? tr("Decompressed again, they are the file.")
                                : tr("Decompressed again, they are not the file: they are not to be used."));
        result_->setText(text);
        // Where the loader would load it, if it is to have one.
        const tuxcmp::cpc::Loader loader = tuxcmp::cpc::suggestLoader(name_->text().toStdString(), destination(), entry(),
                                                                      static_cast<int>(job->data.size()), *packed_);
        load_->setText(addressText(loader.source));
    }
    updateButtons();
    emit compressed();
}

void CompressorWindow::showProgress()
{
    if (!job_)
        return;
    const int total = job_->total;
    // The search slows as it goes: each byte is compared with more of those
    // before it, up to 32640. The bar follows the work, not the bytes.
    const auto work = [](double bytes) {
        const double limit = tuxcmp::zx0::kMaxOffset;
        return bytes <= limit ? bytes * bytes / 2 : limit * limit / 2 + (bytes - limit) * limit;
    };
    if (total > 0)
        progress_->setValue(static_cast<int>(1000 * work(job_->done) / work(total)));
}

void CompressorWindow::forget()
{
    packed_.reset();
    checked_ = false;
    result_->clear();
    load_->clear();
    updateButtons();
}

QString CompressorWindow::packedName() const
{
    return name_->text().trimmed().toUpper();
}

void CompressorWindow::setPackedName(const QString& name)
{
    name_->setText(name);
}

bool CompressorWindow::withHeader() const
{
    return headerBox_->isChecked();
}

void CompressorWindow::setWithHeader(bool on)
{
    headerBox_->setChecked(on);
}

std::optional<uint16_t> CompressorWindow::typedLoad() const
{
    return readAddress(load_->text());
}

uint16_t CompressorWindow::loadAddress() const
{
    return typedLoad().value_or(0);
}

void CompressorWindow::setLoadAddress(uint16_t address)
{
    load_->setText(addressText(address));
}

QByteArray CompressorWindow::packedFile() const
{
    if (!packed_)
        return {};
    std::vector<uint8_t> file = packed_->data;
    if (withHeader())
        file = tuxcmp::cpc::withHeader(packedName().toStdString(), {tuxcmp::cpc::kBinary, loadAddress(), 0, 0}, packed_->data);
    return QByteArray(reinterpret_cast<const char*>(file.data()), static_cast<qsizetype>(file.size()));
}

uint16_t CompressorWindow::destination() const
{
    if (header_ && skipHeader_->isChecked())
        return header_->loadAddress;
    const QString suffix = QFileInfo(path_).suffix().toUpper();
    const size_t size = data().size();
    if (size <= 0x4000 && (size == 0x4000 || suffix == QLatin1String("SCR")))
        return tuxcmp::cpc::kScreen;
    return 0x4000;
}

std::optional<uint16_t> CompressorWindow::entry() const
{
    // A program says where it is run; a picture is not run.
    if (!header_ || !skipHeader_->isChecked() || header_->type != tuxcmp::cpc::kBinary || !header_->entryAddress)
        return std::nullopt;
    const int start = header_->loadAddress, end = start + static_cast<int>(data().size());
    if (start >= tuxcmp::cpc::kScreen || header_->entryAddress < start || header_->entryAddress >= end)
        return std::nullopt;
    return header_->entryAddress;
}

bool CompressorWindow::putOn(const DiscTarget& target, bool askForLoader)
{
    refreshDiscs();
    if (!packed_ || !checked_)
        return false;
    const QString name = packedName();
    if (!discs::put(host_, target, name, packedFile(), false, this))
        return false;
    setStatus(tr("%1 is on %2.").arg(name, discs::describe(target)));
    refreshDiscs();
    if (askForLoader
        && prompts::ask(this, tr("Decompression Program"),
                        tr("%1 is on %2.\n\nDo you want a program made to decompress it? A BASIC loader, and the "
                           "Z80 program it calls, go on the same disc.")
                            .arg(name, discs::describe(target))))
        makeLoader(target);
    return true;
}

bool CompressorWindow::makeLoader(const DiscTarget& target)
{
    if (!packed_)
        return false;
    const std::vector<uint8_t> original = data();
    const int length = static_cast<int>(original.size());
    tuxcmp::cpc::Loader loader = tuxcmp::cpc::suggestLoader(packedName().toStdString(), destination(), entry(), length, *packed_);
    // Where the file's header already loads it, if that does.
    if (withHeader() && typedLoad()) {
        tuxcmp::cpc::Loader kept = loader;
        kept.source = *typedLoad();
        if (tuxcmp::cpc::works(tuxcmp::cpc::checkLoader(kept, original, packed_->data)))
            loader = kept;
    }
    LoaderDialog dialog(host_, target, original, *packed_, loader, this);
    if (dialog.exec() != QDialog::Accepted)
        return false;
    const tuxcmp::cpc::Loader made = dialog.loader();
    // The compressed file went again on the disc, with the loader's header.
    setWithHeader(true);
    setLoadAddress(made.source);
    setStatus(tr("%1, %2 and %3 are on %4; RUN\"%5\" runs them.")
                  .arg(QString::fromStdString(made.basicName), QString::fromStdString(made.programName),
                       QString::fromStdString(made.packedName), discs::describe(target),
                       QString::fromStdString(made.basicName.substr(0, made.basicName.find('.')))));
    refreshDiscs();
    return true;
}

bool CompressorWindow::saveAs(const QString& path)
{
    if (!packed_)
        return false;
    QFile file(path);
    const QByteArray content = packedFile();
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(content) != content.size()) {
        prompts::problem(this, tr("%1 cannot be written: %2").arg(QDir::toNativeSeparators(path), file.errorString()));
        return false;
    }
    preferences_.setOutputFolder(QFileInfo(path).absolutePath());
    setStatus(tr("%1 is written.").arg(QFileInfo(path).fileName()));
    return true;
}

int CompressorWindow::drive() const
{
    return drives_->currentData().isValid() ? drives_->currentData().toInt() : 0;
}

void CompressorWindow::setDrive(int drive)
{
    const int at = drives_->findData(drive);
    if (at >= 0)
        drives_->setCurrentIndex(at);
    updateButtons();
}

void CompressorWindow::refreshDiscs()
{
    const int chosen = drive();
    drives_->clear();
    const int drives = host_ ? host_->drives() : 0;
    for (int drive = 0; drive < drives; ++drive) {
        const tuxape::PluginDisc disc = host_->disc(drive);
        QString text = tr("Drive %1:").arg(discs::driveLetter(drive)) + ' ';
        if (!disc.present)
            text += tr("no disc");
        else if (!disc.writable)
            text += tr("%1, which files cannot be put on").arg(disc.name.isEmpty() ? tr("a disc") : disc.name);
        else
            text += tr("%1, %2 file(s), %3K free")
                        .arg(disc.name.isEmpty() ? tr("a disc") : disc.name)
                        .arg(disc.files.size())
                        .arg(disc.freeBytes / 1024);
        drives_->addItem(text, drive);
    }
    if (drives == 0)
        drives_->addItem(tr("TuxAPE is not there"));
    const int at = drives_->findData(chosen);
    drives_->setCurrentIndex(at >= 0 ? at : 0);

    switch (discs::tuxDsk(host_)) {
    case discs::TuxDskState::Missing:
        tuxDskInfo_->setText(tr("not loaded: install it, or turn it on, in TuxAPE's Settings, on the Plugins page."));
        break;
    case discs::TuxDskState::TooOld:
        tuxDskInfo_->setText(tr("this version cannot take files from other plugins: install a newer one."));
        break;
    case discs::TuxDskState::Ready: {
        const QString disc = discs::tuxDskDisc(host_);
        tuxDskInfo_->setText(disc.isEmpty() ? tr("its first disc: none yet, one is made for the file.")
                                            : tr("its first disc, %1.").arg(disc));
        break;
    }
    }
    updateButtons();
}

void CompressorWindow::updateButtons()
{
    const bool ready = packed_ && checked_ && !compressing() && tuxcmp::cpc::discName(packedName().toStdString())
                       && (!withHeader() || typedLoad());
    compress_->setEnabled(!file_.empty());
    skipHeader_->setEnabled(header_.has_value() && !compressing());
    load_->setEnabled(withHeader());
    bool driveReady = false;
    if (host_ && drive() < host_->drives()) {
        const tuxape::PluginDisc disc = host_->disc(drive());
        driveReady = disc.present && disc.writable;
    }
    putDrive_->setEnabled(ready && driveReady);
    putTuxDsk_->setEnabled(ready && discs::tuxDsk(host_) == discs::TuxDskState::Ready);
    save_->setEnabled(packed_ && checked_ && !compressing());
}

void CompressorWindow::chooseFile()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("File to Compress"), preferences_.inputFolder(), tr("All files (*)"));
    if (!path.isEmpty())
        openFile(path);
}

void CompressorWindow::chooseSave()
{
    if (!packed_)
        return;
    // PackZX0's name: the file's own, with .ZX0 after it.
    const QString folder = preferences_.outputFolder().isEmpty() ? QFileInfo(path_).absolutePath() : preferences_.outputFolder();
    const QString path = QFileDialog::getSaveFileName(this, tr("Save the Compressed File"),
                                                      QDir(folder).filePath(QFileInfo(path_).fileName() + ".ZX0"),
                                                      tr("ZX0 files (*.zx0);;All files (*)"));
    if (!path.isEmpty())
        saveAs(path);
}

void CompressorWindow::setStatus(const QString& text)
{
    status_->setText(text);
}

QString CompressorWindow::status() const
{
    return status_->text();
}

void CompressorWindow::changeEvent(QEvent* event)
{
    // Back to the window: the discs may have changed in the meantime, and
    // TuxDSK come or gone.
    if (event->type() == QEvent::ActivationChange && isActiveWindow())
        refreshDiscs();
    QDialog::changeEvent(event);
}

void CompressorWindow::dragEnterEvent(QDragEnterEvent* event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    if (urls.size() == 1 && urls.first().isLocalFile())
        event->acceptProposedAction();
}

void CompressorWindow::dropEvent(QDropEvent* event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    if (urls.size() == 1 && urls.first().isLocalFile()) {
        openFile(urls.first().toLocalFile());
        event->acceptProposedAction();
    }
}
