#include "ConfigManager.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>
#include <QStandardPaths>

namespace {

QString legacyConfigDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
        + QStringLiteral("/indicator-debug-tool");
}

QJsonObject readJsonFile(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QJsonObject();

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return QJsonObject();
    return doc.object();
}

} // namespace

QString ConfigManager::configDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
        + QStringLiteral("/cd-debugtool");
}

QString ConfigManager::configFile()
{
    return configDir() + QStringLiteral("/config.json");
}

QJsonObject ConfigManager::defaults()
{
    QJsonObject o;
    o.insert(QStringLiteral("broadcast_ip"), QStringLiteral("192.168.114.200"));
    o.insert(QStringLiteral("listen_port"), 10011);
    o.insert(QStringLiteral("device_port"), 10011);
    o.insert(QStringLiteral("last_firmware_path"), QStringLiteral(""));
    o.insert(QStringLiteral("last_serial_port"), QStringLiteral(""));
    o.insert(QStringLiteral("baud_rate"), 115200);
    o.insert(QStringLiteral("window_geometry"), QJsonValue(QJsonValue::Null));
    o.insert(QStringLiteral("transport_type"), QStringLiteral("UDP"));
    o.insert(QStringLiteral("rs485_device_id"), 1);
    o.insert(QStringLiteral("rs485_frame_interval"), 15);
    o.insert(QStringLiteral("last_tab"), 0);
    return o;
}

QJsonObject ConfigManager::load()
{
    QJsonObject result = defaults();

    if (QFile::exists(configFile())) {
        const QJsonObject saved = readJsonFile(configFile());
        if (saved.isEmpty())
            return result; // corrupt -> defaults
        for (auto it = saved.constBegin(); it != saved.constEnd(); ++it)
            result.insert(it.key(), it.value());
        return result;
    }

    // Legacy migration: one-time copy of known keys from indicator-debug-tool.
    const QString legacyFile = legacyConfigDir() + QStringLiteral("/config.json");
    if (QFile::exists(legacyFile)) {
        const QJsonObject old = readJsonFile(legacyFile);
        if (!old.isEmpty()) {
            const QJsonObject defs = defaults();
            for (auto it = defs.constBegin(); it != defs.constEnd(); ++it) {
                if (old.contains(it.key()))
                    result.insert(it.key(), old.value(it.key()));
            }
            save(result);
        }
    }
    return result;
}

void ConfigManager::save(const QJsonObject& o)
{
    QDir().mkpath(configDir());
    QFile f(configFile());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
}

QString ConfigManager::getString(const QJsonObject& o, const char* key, const QString& def)
{
    const QJsonValue v = o.value(QLatin1String(key));
    return v.isString() ? v.toString() : def;
}

int ConfigManager::getInt(const QJsonObject& o, const char* key, int def)
{
    const QJsonValue v = o.value(QLatin1String(key));
    return v.isDouble() ? v.toInt() : def;
}

bool ConfigManager::getBool(const QJsonObject& o, const char* key, bool def)
{
    const QJsonValue v = o.value(QLatin1String(key));
    return v.isBool() ? v.toBool() : def;
}
