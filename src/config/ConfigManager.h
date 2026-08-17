#pragma once
#include <QJsonObject>
#include <QString>

// ConfigManager: QJsonDocument/QJsonObject based persistence (no third-party
// deps). Configuration lives in ~/.config/cd-debugtool/config.json, with a
// one-time best-effort migration from the legacy indicator-debug-tool location.
class ConfigManager {
public:
    static QString configDir();
    static QString configFile();

    static QJsonObject load();
    static void save(const QJsonObject& o);

    static QJsonObject defaults();

    static QString getString(const QJsonObject& o, const char* key, const QString& def);
    static int getInt(const QJsonObject& o, const char* key, int def);
    static bool getBool(const QJsonObject& o, const char* key, bool def);
};
