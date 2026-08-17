#pragma once
#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QThread>
#include <QVector>
#include <QAtomicInteger>

#include <functional>
#include <utility>

#include "Device.h"

class UpgradeWorker;

enum class EngineState {
    Idle,
    Discovering,
    Querying,
    Erasing,
    Transferring,
    Verifying,
    Done,
    Failed,
};

inline QString engineStateLabel(EngineState s)
{
    switch (s) {
    case EngineState::Idle: return QStringLiteral("空闲");
    case EngineState::Discovering: return QStringLiteral("扫描中");
    case EngineState::Querying: return QStringLiteral("查询固件");
    case EngineState::Erasing: return QStringLiteral("擦除中");
    case EngineState::Transferring: return QStringLiteral("传输中");
    case EngineState::Verifying: return QStringLiteral("验证中");
    case EngineState::Done: return QStringLiteral("完成");
    case EngineState::Failed: return QStringLiteral("失败");
    }
    return QString();
}

// Per-device upgrade worker. Runs the erase -> transfer -> verify state machine
// inside a dedicated QThread. All UI-visible transitions are emitted as queued
// signals; the only cross-thread state read is the (atomic) device status.
class UpgradeWorker : public QThread {
    Q_OBJECT
public:
    using SendFunc = std::function<void(const QByteArray&, Device*)>;

    UpgradeWorker(Device* device, const QByteArray& firmware, QObject* parent = nullptr);

    void setSendFunc(SendFunc func) { m_sendFunc = std::move(func); }
    void cancel() { m_cancel.storeRelaxed(1); }
    void pause() { m_paused.storeRelaxed(1); }
    void resume() { m_paused.storeRelaxed(0); }

signals:
    void progressUpdated(const QString& deviceId, double percent, int currentPkt, int totalPkt);
    void statusChanged(const QString& deviceId, DeviceStatus status);
    void logMessage(const QString& msg);
    void finishedSignal(const QString& deviceId, bool success);

protected:
    void run() override;

private:
    bool waitForStatus(DeviceStatus target, int timeoutSec);
    void send(const QByteArray& data);

    Device* m_device;
    QVector<quint32> m_firmwareWords;
    quint32 m_firmwareSizeBytes = 0;
    int m_totalPackets = 0;
    SendFunc m_sendFunc;
    QAtomicInteger<int> m_cancel{0};
    QAtomicInteger<int> m_paused{0};
};

// Orchestrates multi-device upgrades. Owns one UpgradeWorker per device and
// aggregates their results into a single Done/Failed outcome.
class UpgradeEngine : public QObject {
    Q_OBJECT
public:
    using SendFunc = std::function<void(const QByteArray&, Device*)>;

    explicit UpgradeEngine(QObject* parent = nullptr);

    void setSendFunc(SendFunc func) { m_sendFunc = std::move(func); }
    QJsonObject loadFirmware(const QString& filepath);
    void startUpgrade(const QVector<Device*>& devices);
    void cancelAll();
    void pauseAll();
    void resumeAll();
    // 取消并同步等待所有 worker 线程结束（切页/析构前调用，
    // 避免“升级中销毁 → QThread destroyed while running”崩溃）。
    void waitForWorkers();

    EngineState state() const { return m_state; }
    bool isRunning() const;

signals:
    void logMessage(const QString& msg);
    void progressUpdated(const QString& deviceId, double percent, int current, int total);
    void deviceStatusChanged(const QString& deviceId, DeviceStatus status);
    void engineStateChanged(EngineState state);
    void allFinished(const QHash<QString, bool>& results);

private slots:
    void onWorkerFinished(const QString& deviceId, bool success);

private:
    EngineState m_state = EngineState::Idle;
    QHash<QString, UpgradeWorker*> m_workers;
    QHash<QString, bool> m_results;
    int m_totalWorkers = 0;
    QByteArray m_firmware;
    SendFunc m_sendFunc;
};
