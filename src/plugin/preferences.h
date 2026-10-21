#pragma once

#include <QHash>
#include <QString>
#include <QVariant>

// What TuxCMP remembers from one day to the next: the folder files were
// last taken from and saved to, and the choices of its windows. They are
// kept in TuxCMP.ini, in the folder TuxAPE gives the plugin for its own;
// with no such folder, as when a test shows a window by itself, they are
// remembered for as long as the window lives.
class Preferences {
public:
    // `folder` is PluginHost::dataFolder(), or empty.
    explicit Preferences(const QString& folder = QString());

    QString file() const { return file_; }

    QString inputFolder() const { return text("Folders/Input"); }
    void setInputFolder(const QString& folder) { set("Folders/Input", folder); }
    QString outputFolder() const { return text("Folders/Output"); }
    void setOutputFolder(const QString& folder) { set("Folders/Output", folder); }

    // A file with an AMSDOS header is compressed without it.
    bool skipHeader() const { return value("Compressor/SkipHeader", true).toBool(); }
    void setSkipHeader(bool on) { set("Compressor/SkipHeader", on); }
    // The compressed file is given one.
    bool header() const { return value("Compressor/Header", true).toBool(); }
    void setHeader(bool on) { set("Compressor/Header", on); }
    // The drive of TuxAPE files are put in.
    int drive() const { return value("Compressor/Drive", 0).toInt(); }
    void setDrive(int drive) { set("Compressor/Drive", drive); }

private:
    QVariant value(const char* key, const QVariant& fallback) const { return kept_.value(QLatin1String(key), fallback); }
    QString text(const char* key) const { return value(key, QString()).toString(); }
    void set(const char* key, const QVariant& value);

    QString file_;
    QHash<QString, QVariant> kept_;
};
