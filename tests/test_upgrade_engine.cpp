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