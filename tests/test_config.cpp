#include <QtTest>

#include <QDir>
#include <QFile>
#include <QJsonObject>
#include <QStandardPaths>

#include "config/ConfigManager.h"

// ConfigManager 纯逻辑与持久化往返测试。
// initTestCase 开启 QStandardPaths 测试模式，读写隔离，不污染真实用户配置。
class TestConfig : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void testDefaults();
    void testGettersWithFallback();
    void testSaveLoadRoundTrip();
    void testCorruptFileFallsBackToDefaults();
};

void TestConfig::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QFile::remove(ConfigManager::configFile());
}

void TestConfig::testDefaults()
{
    const QJsonObject d = ConfigManager::defaults();
    QCOMPARE(ConfigManager::getInt(d, "listen_port", 0), 10011);
    QCOMPARE(ConfigManager::getInt(d, "device_port", 0), 10011);
    QCOMPARE(ConfigManager::getInt(d, "baud_rate", 0), 115200);
    QCOMPARE(ConfigManager::getInt(d, "last_tab", -1), 0);
    QCOMPARE(ConfigManager::getString(d, "broadcast_ip", QString()),
             QStringLiteral("192.168.114.200"));
    QCOMPARE(ConfigManager::getString(d, "last_firmware_path", QStringLiteral("x")),
             QStringLiteral(""));
    QCOMPARE(ConfigManager::getInt(d, "rs485_device_id", 0), 1);
    QCOMPARE(ConfigManager::getInt(d, "rs485_frame_interval", 0), 15);
}

void TestConfig::testGettersWithFallback()
{
    // 类型不符或缺失时回退调用方提供的默认值。
    QJsonObject o;
    o.insert(QStringLiteral("baud_rate"), QStringLiteral("not-a-number"));
    QCOMPARE(ConfigManager::getInt(o, "baud_rate", 42), 42);

    o.insert(QStringLiteral("last_firmware_path"), 123);
    QCOMPARE(ConfigManager::getString(o, "last_firmware_path", QStringLiteral("fallback")),
             QStringLiteral("fallback"));

    QCOMPARE(ConfigManager::getBool(o, "nonexistent", true), true);

    // 正确类型时取配置值。
    o.insert(QStringLiteral("last_serial_port"), QStringLiteral("/dev/ttyUSB0"));
    QCOMPARE(ConfigManager::getString(o, "last_serial_port", QString()),
             QStringLiteral("/dev/ttyUSB0"));
    o.insert(QStringLiteral("baud_rate"), 921600);
    QCOMPARE(ConfigManager::getInt(o, "baud_rate", 0), 921600);
    o.insert(QStringLiteral("flag"), false);
    QCOMPARE(ConfigManager::getBool(o, "flag", true), false);
}

void TestConfig::testSaveLoadRoundTrip()
{
    QFile::remove(ConfigManager::configFile());

    QJsonObject cfg = ConfigManager::load(); // 无文件 → defaults
    QCOMPARE(ConfigManager::getString(cfg, "last_serial_port", QStringLiteral("z")),
             QStringLiteral(""));

    cfg.insert(QStringLiteral("last_serial_port"), QStringLiteral("/dev/ttyACM0"));
    cfg.insert(QStringLiteral("baud_rate"), 921600);
    cfg.insert(QStringLiteral("last_firmware_path"), QStringLiteral("/tmp/fw.bin"));
    cfg.insert(QStringLiteral("window_geometry"), QStringLiteral("AAECAwQ="));
    cfg.insert(QStringLiteral("last_tab"), 3);
    ConfigManager::save(cfg);

    const QJsonObject loaded = ConfigManager::load();
    QCOMPARE(ConfigManager::getString(loaded, "last_serial_port", QString()),
             QStringLiteral("/dev/ttyACM0"));
    QCOMPARE(ConfigManager::getInt(loaded, "baud_rate", 0), 921600);
    QCOMPARE(ConfigManager::getString(loaded, "last_firmware_path", QString()),
             QStringLiteral("/tmp/fw.bin"));
    QCOMPARE(ConfigManager::getString(loaded, "window_geometry", QString()),
             QStringLiteral("AAECAwQ="));
    QCOMPARE(ConfigManager::getInt(loaded, "last_tab", 0), 3);

    // 未写入的键保持默认值。
    QCOMPARE(ConfigManager::getInt(loaded, "listen_port", 0), 10011);
    QCOMPARE(ConfigManager::getInt(loaded, "device_port", 0), 10011);
    QCOMPARE(ConfigManager::getString(loaded, "broadcast_ip", QString()),
             QStringLiteral("192.168.114.200"));

    QFile::remove(ConfigManager::configFile());
}

void TestConfig::testCorruptFileFallsBackToDefaults()
{
    QDir().mkpath(ConfigManager::configDir());
    {
        QFile f(ConfigManager::configFile());
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write("{ not valid json");
    }

    const QJsonObject cfg = ConfigManager::load();
    QCOMPARE(ConfigManager::getInt(cfg, "listen_port", 0), 10011); // 回退默认
    QCOMPARE(ConfigManager::getInt(cfg, "baud_rate", 0), 115200);

    QFile::remove(ConfigManager::configFile());
}

int runTestConfig(int argc, char** argv)
{
    TestConfig test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_config.moc"