#pragma once

#include <QAction>
#include <QMainWindow>
#include <QMap>
#include <QObject>
#include <QPointer>
#include <QStringList>

#include "plugin.h"

// A TuxAPE of the tests' own: every function of PluginHost, of every
// version, noting what the plugin asks of it. Its two drives' discs are
// lists of files; TuxDSK is whatever object the test says it is.
class Host : public tuxape::PluginHost {
public:
    explicit Host(const QString& folder) : folder_(folder) {}

    struct Drive {
        bool present = false;
        bool writable = true;
        QString name;
        QStringList order;  // the files, in the order they went on
        QMap<QString, QByteArray> files;
    };

    QMainWindow frame;
    QList<QAction*> actions;
    QList<QAction*> tilesActions;
    int buttons = 0;
    int pictures = 0;       // the buttons' pictures given, again when the look changes
    QStringList typed;      // what was typed on the CPC's keyboard
    QStringList assembled;  // the sources shown in the assembler
    tuxape::PluginTiles tiles;
    Drive drive[2];
    QPointer<QObject> tuxDsk;
    QString speaks = QStringLiteral("en");

    // ---- Version 1 ----
    QWidget* window() override { return &frame; }
    QAction* addAction(const QString& text, const QKeySequence& shortcut) override
    {
        auto* action = new QAction(text, &frame);
        action->setShortcut(shortcut);
        actions << action;
        return action;
    }
    void addButton(QAction* action, const QIcon& icon) override
    {
        if (actions.contains(action) && !icon.isNull())
            ++pictures;
        buttons = 1;
    }
    bool insertDisc(int, const QString&) override { return false; }
    bool loadSnapshot(const QString&) override { return false; }
    void autoType(const QString& text) override { typed << text; }
    void reset(bool) override {}
    QByteArray readMemory(int, int size) override { return QByteArray(size, '\0'); }
    tuxape::PluginScreen screen() override { return {}; }
    void openInAssembler(const QString& path) override { assembled << path; }
    QString dataFolder() override { return folder_; }
    bool assembleAndRun(const QString&) override { return false; }

    // ---- Version 2 ----
    QAction* addTilesAction(const QString& text) override
    {
        auto* action = new QAction(text, &frame);
        tilesActions << action;
        return action;
    }
    tuxape::PluginTiles selectedTiles() override { return tiles; }

    // ---- Version 3 ----
    int drives() override { return 2; }
    tuxape::PluginDisc disc(int which) override
    {
        tuxape::PluginDisc disc;
        if (which < 0 || which > 1 || !drive[which].present)
            return disc;
        disc.present = true;
        disc.writable = drive[which].writable;
        disc.name = drive[which].name;
        disc.files = drive[which].order;
        disc.freeBytes = 178 * 1024;
        return disc;
    }
    QString putFile(int which, const QString& name, const QByteArray& content) override
    {
        if (which < 0 || which > 1 || !drive[which].present)
            return QStringLiteral("There is no disc in drive %1:.").arg(QChar('A' + which));
        if (!drive[which].writable)
            return QStringLiteral("The disc in drive %1: cannot be written to.").arg(QChar('A' + which));
        if (!drive[which].files.contains(name))
            drive[which].order << name;
        drive[which].files[name] = content;
        return {};
    }
    QObject* plugin(const QString& id) override { return id == "tuxdsk" ? tuxDsk.data() : nullptr; }

    // ---- Version 4 ----
    QString language() override { return speaks; }

private:
    QString folder_;
};
