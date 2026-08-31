#include <QtTest>

#include <QCoreApplication>
#include <QFile>
#include <QHash>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <QVariant>

#include "core/UpgradeEngine.h"

// UpgradeEngine 取消路径聚合测试：
// P0-1 修复后，取消升级必须照常发射 finishedSignal(id, false)，
// 使 onWorkerFinished 聚合收尾（allFinished 正常发出）。
class TestUpgradeEngine : public QObject {
    Q_OBJECT

private slots:
    void testLoadFirmwareInfo();
    void testLoadFirmwareCrc();
    void testLoadFirmwareIntelHex();
    void testCancelCompletesAggregation();
};

void TestUpgradeEngine::testLoadFirmwareInfo()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString fwPath = dir.filePath(QStringLiteral("fw.bin"));
    {
        QFile f(fwPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QByteArray(4096, char(0xAB)));
    }

    UpgradeEngine engine;
    const QJsonObject info = engine.loadFirmware(fwPath);
    QVERIFY(!info.isEmpty());
    QCOMPARE(static_cast<int>(info.value(QStringLiteral("size")).toDouble()), 4096);
    QCOMPARE(static_cast<int>(info.value(QStringLiteral("sizeWords")).toDouble()), 1024);
    QVERIFY(info.value(QStringLiteral("totalPackets")).toDouble() > 0);
    QVERIFY(info.value(QStringLiteral("crc")).toDouble() > 0);
}

void TestUpgradeEngine::testLoadFirmwareCrc()
{
    // 固件 CRC = 0xFF 填充到 4B 对齐 + crc32Mpeg2Words（word 流大端 MPEG-2），
    // 与设备 Recovery HAL_CRC_Calculate 及 Java CRC32_OR_MPEG_2(int[]) 一致。
    // 4B 对齐时填充为空：4096 字节 0xAB → 1024 word 0xABABABAB → 0xAA61A1F3（独立 Python 实现算出）。
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString fwPath = dir.filePath(QStringLiteral("fw.bin"));
    {
        QFile f(fwPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QByteArray(4096, char(0xAB)));
    }

    UpgradeEngine engine;
    const QJsonObject info = engine.loadFirmware(fwPath);
    QCOMPARE(static_cast<quint32>(info.value(QStringLiteral("crc")).toDouble()),
             static_cast<quint32>(0xAA61A1F3u));
}

void TestUpgradeEngine::testLoadFirmwareIntelHex()
{
    // Intel HEX 加载：两条数据记录（addr 0 四字节 + addr 4 单字节 0x05）+
    // EOF。解析后与 .bin 字节流等价：size=5、CRC=0xCCD0E62C——0xFF 填充后
    // 按小端读出 words {0x04030201, 0xFFFFFF05} 再 crc32Mpeg2Words
    // （= 设备 Recovery HAL_CRC_Calculate 对该 flash word 流的 CRC）。
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString hexPath = dir.filePath(QStringLiteral("fw.hex"));
    {
        QFile f(hexPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        // 校验和：记录 1 sum=0x0E→CC=F2；记录 2 sum=0x0A→CC=F6。
        f.write(":0400000001020304F2\r\n:0100040005F6\r\n:00000001FF\r\n");
    }

    UpgradeEngine engine;
    const QJsonObject info = engine.loadFirmware(hexPath);
    QVERIFY(!info.isEmpty());
    QCOMPARE(static_cast<int>(info.value(QStringLiteral("size")).toDouble()), 5);
    QCOMPARE(static_cast<int>(info.value(QStringLiteral("totalPackets")).toDouble()), 1);
    QCOMPARE(static_cast<quint32>(info.value(QStringLiteral("crc")).toDouble()),
             static_cast<quint32>(0xCCD0E62Cu));

    // 坏校验和的 .hex 必须拒绝（返回空 info）。
    const QString badPath = dir.filePath(QStringLiteral("bad.hex"));
    {
        QFile f(badPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(":0400000001020304F3\r\n:00000001FF\r\n");
    }
    QVERIFY(engine.loadFirmware(badPath).isEmpty());
}

void TestUpgradeEngine::testCancelCompletesAggregation()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString fwPath = dir.filePath(QStringLiteral("fw.bin"));
    {
        QFile f(fwPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QByteArray(4096, char(0xAB)));
    }

    UpgradeEngine engine;
    QVERIFY(!engine.loadFirmware(fwPath).isEmpty());

    Device dev;
    dev.deviceId = QStringLiteral("dev-test");
    dev.transport = TransportType::Serial;
    // 设备始终停留在 Offline：擦除后 waitForStatus 不会进入 Transferring，
    // worker 将停留在等待循环，随后被 cancel 打断。
    dev.status.store(DeviceStatus::Offline);

    engine.setSendFunc([](const QByteArray&, Device*) {});

    QSignalSpy spyFinished(&engine, &UpgradeEngine::allFinished);
    QSignalSpy spyState(&engine, &UpgradeEngine::engineStateChanged);

    engine.startUpgrade({&dev});
    QVERIFY(engine.isRunning());

    // 稍等片刻后取消：worker 在 waitForStatus(200ms 轮询) 中退出。
    QTimer::singleShot(50, &engine, [&engine]() { engine.cancelAll(); });

    QVERIFY(spyFinished.wait(5000));
    const QVariantList args = spyFinished.takeFirst();
    const QHash<QString, bool> results = args.at(0).value<QHash<QString, bool>>();
    QCOMPARE(results.size(), 1);
    QCOMPARE(results.value(QStringLiteral("dev-test")), false);

    // 聚合收尾后引擎必须回到终态（Failed），而不是卡在运行态。
    QVERIFY(!engine.isRunning());
    QCOMPARE(engine.state(), EngineState::Failed);
    QVERIFY(spyState.count() >= 2); // Transferring + Failed（+ cancelAll 的 Idle）

    // 确保 worker 线程彻底退出后再让 dev 离开作用域。
    engine.waitForWorkers();
}

int runTestUpgradeEngine(int argc, char** argv)
{
    // 本组测试涉及跨线程信号、单次定时器与事件处理，
    // 需要 QCoreApplication 提供事件循环（tests 入口未创建）。
    QCoreApplication app(argc, argv);
    TestUpgradeEngine test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_upgrade_engine.moc"