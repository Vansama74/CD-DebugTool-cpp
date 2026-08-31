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

// Per-device upgrade worker. Runs the full IAP upgrade state machine inside a
// dedicated QThread:
//   4B06 进入 Recovery（ACK 确认）→ 4B07 重启 → 等设备重新上线（4B01 广播）
//   → 4B04 擦除 → 4B05 传输（整轮发送 + 缺失帧重传）→ 4B03 轮询验证
//   → 4B07 重启回主固件。
// 设备只对「到达 max_len 帧」的整轮发送应答 rtn_cmd05（见 Recovery
// cmd.c cmd_SendUpgradePackage_05），故传输以「每轮 N 帧」为单位推进。
// All UI-visible transitions are emitted as queued signals; the only
// cross-thread state read is the (atomic) device status / 邮箱计数。
class UpgradeWorker : public QThread {
    Q_OBJECT
public:
    using SendFunc = std::function<void(const QByteArray&, Device*)>;
    // 广播函数（UDP 模式）：重启后用于 4B01 搜索确认设备重新上线。串口模式不设置。
    using BroadcastFunc = std::function<void(const QByteArray&)>;

    UpgradeWorker(Device* device, const QByteArray& firmware, QObject* parent = nullptr);

    void setSendFunc(SendFunc func) { m_sendFunc = std::move(func); }
    void setBroadcastFunc(BroadcastFunc func) { m_broadcastFunc = std::move(func); }
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
    bool enterRecoveryPhase(const QString& id, const QString& name);
    bool erasePhase(const QString& id, const QString& name);
    bool transferPhase(const QString& id, const QString& name);
    bool verifyPhase(const QString& id, const QString& name);
    void failPhase(const QString& id, const QString& name, const QString& reason);

    // 轮询原子计数：值相对 baseline 变化即返回 true。
    bool waitForStamp(const std::atomic<quint32>& stamp, quint32 baseline, int timeoutSec);
    // 读取最近一次 rtn_cmd05 的缺失帧索引列表（0-based，空=全部收到）。
    QVector<quint32> takeMissList() const;
    // 构造某一轮的发送序号（每轮恒为 N 帧，保证设备 frame_cnt 到达 max_len
    // 而触发一次新应答）：第 0 轮 1..N；重传轮先发缺失帧再补齐 1..N。
    QVector<quint32> buildRoundSeqs(const QVector<quint32>& missing) const;

    void send(const QByteArray& data);

    Device* m_device;
    QVector<quint32> m_firmwareWords;
    quint32 m_firmwareSizeBytes = 0;
    quint32 m_firmwareCrc = 0; // 0xFF 填充 + crc32Mpeg2Words（与设备 Recovery CRC 一致）
    int m_totalPackets = 0;
    SendFunc m_sendFunc;
    BroadcastFunc m_broadcastFunc;
    QAtomicInteger<int> m_cancel{0};
    QAtomicInteger<int> m_paused{0};
};

// Orchestrates multi-device upgrades. Owns one UpgradeWorker per device and
// aggregates their results into a single Done/Failed outcome.
class UpgradeEngine : public QObject {
    Q_OBJECT
public:
    using SendFunc = std::function<void(const QByteArray&, Device*)>;
    using BroadcastFunc = std::function<void(const QByteArray&)>;

    explicit UpgradeEngine(QObject* parent = nullptr);

    void setSendFunc(SendFunc func) { m_sendFunc = std::move(func); }
    void setBroadcastFunc(BroadcastFunc func) { m_broadcastFunc = std::move(func); }
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
    BroadcastFunc m_broadcastFunc;
};