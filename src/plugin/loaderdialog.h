#pragma once

#include <cstdint>
#include <vector>

#include <QDialog>

#include "cpc/loader.h"
#include "discs.h"
#include "zx0/zx0.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
namespace tuxape {
class PluginHost;
}

namespace tuxcmp {

// The window that makes the program to decompress a file: where the data
// go back and whether they are run there, where the loader puts the
// compressed file and the Z80 program, what the files are called. It shows
// the BASIC loader as it will be, and what is wrong with it: the
// decompression is tried, as the CPC will run it, at each change.
//
// Accepted, it puts the three files on the disc the compressed file went
// on: the compressed file again, its header loading it where the loader
// does, the loader and the program.
class LoaderDialog : public QDialog {
    Q_OBJECT

public:
    LoaderDialog(tuxape::PluginHost* host, const DiscTarget& target, std::vector<uint8_t> original,
                 tuxcmp::zx0::Packed packed, const tuxcmp::cpc::Loader& loader, QWidget* parent = nullptr);

    // The loader as the window has it; an address that does not read keeps
    // what it was.
    tuxcmp::cpc::Loader loader() const;
    void setLoader(const tuxcmp::cpc::Loader& loader);
    // Where the compressed file and the program go, chosen again.
    void placeAutomatically();
    std::vector<tuxcmp::cpc::Finding> findings() const;
    // An address typed that does not read.
    bool addressesRead() const;
    QString listing() const;

    // The loader typed on the CPC's keyboard once it is on the disc in a
    // drive: RUN"SHOP".
    bool runAfter() const;
    void setRunAfter(bool on);

    // The program's source, for TuxAPE's assembler.
    QString source() const;
    bool saveSource(const QString& path);
    void showInAssembler();
    void chooseSaveSource();

    // The three files put on the disc; false if one of them did not go.
    bool putFiles();

    // What a finding means, to be read.
    static QString describe(const tuxcmp::cpc::Finding& finding);

public slots:
    void accept() override;

private:
    tuxape::PluginHost* host_;
    DiscTarget target_;
    std::vector<uint8_t> original_;
    tuxcmp::zx0::Packed packed_;
    tuxcmp::cpc::Loader loader_;

    QLineEdit* destination_ = nullptr;
    QLabel* destinationInfo_ = nullptr;
    QCheckBox* run_ = nullptr;
    QLineEdit* entry_ = nullptr;
    QComboBox* mode_ = nullptr;
    QCheckBox* waitKey_ = nullptr;
    QLineEdit* basicName_ = nullptr;
    QLineEdit* programName_ = nullptr;
    QLineEdit* program_ = nullptr;
    QLabel* programInfo_ = nullptr;
    QLabel* packedName_ = nullptr;
    QLineEdit* source_ = nullptr;
    QLabel* packedInfo_ = nullptr;
    QPlainTextEdit* listing_ = nullptr;
    QLabel* findings_ = nullptr;
    QCheckBox* runAfter_ = nullptr;
    QPushButton* ok_ = nullptr;

    void showLoader();
    void update();
};

}  // namespace tuxcmp
