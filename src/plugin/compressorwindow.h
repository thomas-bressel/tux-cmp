#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include <QDialog>
#include <QString>

#include "cpc/amsdos.h"
#include "discs.h"
#include "preferences.h"
#include "zx0/zx0.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QThread;
class QTimer;
namespace tuxape {
class PluginHost;
}

namespace tuxcmp {

// TuxCMP's window: PackZX0 with somewhere to put what it makes.
//
// A file of the computer is chosen, compressed with ZX0 (in the background:
// a large file takes some seconds) and the result checked by decompressing
// it. The compressed file then goes on the disc in one of TuxAPE's drives,
// on TuxDSK's disc if TuxDSK is loaded, or to a file of the computer. Once
// it is on a disc, the window asks whether a program is to be made beside it
// to decompress it: a BASIC loader and the Z80 program it calls.
//
// What a button does by asking (a file, a confirmation) is also there as a
// function that is told: the windows' tests use those.
class CompressorWindow : public QDialog {
    Q_OBJECT

public:
    // `host` is TuxAPE, for its drives and for TuxDSK; it may be null, as it
    // is when a test shows the window by itself.
    explicit CompressorWindow(tuxape::PluginHost* host, QWidget* parent = nullptr);
    ~CompressorWindow() override;

    // ---- The file ----
    bool openFile(const QString& path);
    QString path() const { return path_; }
    // What is compressed: the file, without its AMSDOS header if it has one
    // and the box says so.
    std::vector<uint8_t> data() const;
    std::optional<tuxcmp::cpc::Header> fileHeader() const { return header_; }

    // ---- The compression ----
    // Started in the background; compressed() says when it is done.
    void compress();
    bool compressing() const { return thread_ != nullptr; }
    // Stops it; nothing is left of it.
    void stopCompressing();
    const std::optional<tuxcmp::zx0::Packed>& packed() const { return packed_; }

    // ---- What becomes of it ----
    // Its name on a disc, and its AMSDOS header: whether it has one, and
    // where it loads.
    QString packedName() const;
    void setPackedName(const QString& name);
    bool withHeader() const;
    void setWithHeader(bool on);
    uint16_t loadAddress() const;
    void setLoadAddress(uint16_t address);
    // The file as it goes on a disc or to the computer.
    QByteArray packedFile() const;

    // On a disc, then the question about the loader, unless `askForLoader`
    // is false; false if it did not go on.
    bool putOn(const DiscTarget& target, bool askForLoader = true);
    bool saveAs(const QString& path);
    // The window that makes a loader for the file put on `target`; true if
    // it put one there.
    bool makeLoader(const DiscTarget& target);

    // Where the data go back, if nothing says otherwise, and where they are
    // run, if the header says: as the engine chooses them
    // (cpc::defaultDestination, cpc::defaultEntry).
    uint16_t destination() const;
    std::optional<uint16_t> entry() const;

    // The drive files go to, as chosen in the window, and TuxDSK as it was
    // found when the window was last shown or chosen.
    int drive() const;
    void setDrive(int drive);
    void refreshDiscs();

    void chooseFile();
    void chooseSave();

    void setStatus(const QString& text);
    QString status() const;

signals:
    // The compression is over: done, or stopped.
    void compressed();

protected:
    void changeEvent(QEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    struct Job {
        std::vector<uint8_t> data;
        std::atomic<int> done{0};
        std::atomic<int> total{0};
        std::atomic<bool> stop{false};
        std::optional<tuxcmp::zx0::Packed> result;
    };

    tuxape::PluginHost* host_;
    Preferences preferences_;
    QString path_;
    std::vector<uint8_t> file_;
    std::optional<tuxcmp::cpc::Header> header_;
    std::optional<tuxcmp::zx0::Packed> packed_;
    bool checked_ = false;  // the compressed data decompress to the file
    std::shared_ptr<Job> job_;
    QThread* thread_ = nullptr;
    int generation_ = 0;  // of the compression under way
    QTimer* progressTimer_ = nullptr;

    QLineEdit* fileEdit_ = nullptr;
    QLabel* fileInfo_ = nullptr;
    QCheckBox* skipHeader_ = nullptr;
    QPushButton* compress_ = nullptr;
    QProgressBar* progress_ = nullptr;
    QLabel* result_ = nullptr;
    QLineEdit* name_ = nullptr;
    QCheckBox* headerBox_ = nullptr;
    QLineEdit* load_ = nullptr;
    QComboBox* drives_ = nullptr;
    QPushButton* putDrive_ = nullptr;
    QLabel* tuxDskInfo_ = nullptr;
    QPushButton* putTuxDsk_ = nullptr;
    QPushButton* save_ = nullptr;
    QLabel* status_ = nullptr;

    void finished();
    void showProgress();
    void forget();
    void updateButtons();
    std::optional<uint16_t> typedLoad() const;
    // The file's header, if what is compressed is without it.
    std::optional<tuxcmp::cpc::Header> skippedHeader() const;
};

}  // namespace tuxcmp
