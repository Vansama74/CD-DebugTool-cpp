#pragma once
#include <QString>

class ThemeManager {
public:
    enum class Theme { Dark, Light };

    static Theme detect();
    static QString loadQss(Theme t);
};
