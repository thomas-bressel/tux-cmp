#pragma once

#include <QCoreApplication>
#include <QTranslator>

#include "check.h"

// The windows' tests run in English as TuxAPE runs the plugin in English:
// with English's plurals, "1 file", "3 files", the translation the
// plugin's build makes (TUXCMP_ENGLISH). For as long as it lives.
class English {
public:
    English()
    {
        loaded_ = translator_.load(QStringLiteral(TUXCMP_ENGLISH));
        CHECK(loaded_);
        if (loaded_)
            QCoreApplication::installTranslator(&translator_);
    }
    ~English()
    {
        if (loaded_)
            QCoreApplication::removeTranslator(&translator_);
    }

private:
    QTranslator translator_;
    bool loaded_ = false;
};
