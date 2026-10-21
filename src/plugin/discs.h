#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

class QWidget;
namespace tuxape {
class PluginHost;
}

// Where TuxCMP puts its files: on the disc in one of TuxAPE's drives, or on
// the first disc of TuxDSK's window, if TuxDSK is there to take them.

// One or the other.
struct DiscTarget {
    enum Kind { Drive, TuxDsk };
    Kind kind = Drive;
    int drive = 0;  // for a drive: 0 for A:

    static DiscTarget inDrive(int drive) { return {Drive, drive}; }
    static DiscTarget tuxDsk() { return {TuxDsk, 0}; }
    bool operator==(const DiscTarget&) const = default;
};

namespace discs {

// The id TuxDSK goes by among the plugins, and what it offers TuxCMP: a
// file put on its first disc (a disc made if it has none, a question asked
// before a file goes in place of another unless `replace`), and the name of
// that disc, empty if it has none.
inline constexpr char kTuxDskId[] = "tuxdsk";
inline constexpr char kPutFile[] = "putFile(QString,QByteArray,bool)";
inline constexpr char kDiscName[] = "discName()";

// TuxDSK, as TuxCMP finds it.
enum class TuxDskState {
    Missing,  // not installed, or turned off
    TooOld,   // there, without putFile()
    Ready,
};
TuxDskState tuxDsk(tuxape::PluginHost* host);
// What its first disc is called, or empty if it has none yet.
QString tuxDskDisc(tuxape::PluginHost* host);

// What the user is told of a target: "the disc in drive A:", "TuxDSK's disc".
QString describe(const DiscTarget& target);
QChar driveLetter(int drive);

// A file put on a disc. Before it goes in place of a file the disc has, the
// user is asked, unless `replace`. False if it did not go on: why has been
// said, or the user said not to.
bool put(tuxape::PluginHost* host, const DiscTarget& target, const QString& name, const QByteArray& content,
         bool replace, QWidget* over);

}  // namespace discs
