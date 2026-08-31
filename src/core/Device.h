#pragma once
#include <QString>
#include <QVector>
#include <QtGlobal>

#include <atomic>
#include <mutex>

enum class DeviceStatus {
    Offline,
    Online,
    Erasing,
    Transferring,
    Verifying,
    UpgradeDone,
    UpgradeFailed,
};

inline QString deviceStatusLabel(DeviceStatus status)
{
    switch (status) {
    case DeviceStatus::Offline: return QStringLiteral("离线");
    case DeviceStatus::Online: return QStringLiteral("在线");
    case DeviceStatus::Erasing: return QStringLiteral("擦除中");
    case DeviceStatus::Transferring: return QStringLiteral("传输中");
    case DeviceStatus::Verifying: return QStringLiteral("校验中");
    case DeviceStatus::UpgradeDone: return QStringLiteral("升级完成");
    case DeviceStatus::UpgradeFailed: return QStringLiteral("升级失败");
    }
    return QString();
}

// 设备端 Sector1 update_sta（app_board_update_sta_t：UPDATED=0 /
// UPDATING=1 / FAILED=2）→ UI 文案。4B03 应答 rtn_cmd03 第 11 word
// 上报该字段；-1 表示尚未查询。
inline QString upgradeStateLabel(int updateSta)
{
    switch (updateSta) {
    case 0: return QStringLiteral("升级完成");
    case 1: return QStringLiteral("升级进行中");
    case 2: return QStringLiteral("升级失败");
    default: return QStringLiteral("空闲/未知");
    }
}

enum class TransportType { Udp, Serial };

// Device identity/hash key = deviceId (ip for UDP, serial port for serial).
//
// `status` is atomic because the upgrade worker polls it from its own thread
// while the UI / device-manager thread mutates it (see
// UpgradeWorker::waitForStatus). Making it atomic avoids a data race on the
// enum that the Python reference (GIL-protected) never had to think about.
struct Device {
    QString deviceId;
    QString ip, mask, gateway;
    // 设备 4B01 应答第 4 word 上报的端口 = TCP 业务口（Sector1 net_cfg.port，
    // 如 9528），并非 IAP 口。仅作 UI 显示/第三方工具连接参考；IAP 单播目标
    // 恒为固定 10011（IapCommands::IAP_PORT），切勿用 appPort 作单播目标端口。
    quint16 appPort = 0;
    TransportType transport = TransportType::Udp;
    QString serialPort;
    int baudRate = 115200;
    QString fwVersion;
    // 原子化：DeviceManager（GUI 线程）写入，UpgradeWorker（升级线程）在
    // 验证成功后读取比对（本地固件 CRC/size vs 设备上报值）。
    std::atomic<quint32> fwSize{0};
    std::atomic<quint32> fwCrc{0};
    std::atomic<int> upgradeState{-1};
    std::atomic<DeviceStatus> status{DeviceStatus::Offline};
    int progress = 0;
    bool selected = true;

    // —— 升级 worker 邮箱（DeviceManager GUI 线程写入、UpgradeWorker 线程轮询）——
    // 各字段均为单调递增的应答计数，worker 以「基线值变化」判定应答到达，
    // 避免新增跨线程信号/锁链。miss 列表由 mutex 保护（QVector 非线程安全）。
    std::atomic<quint32> enterRecoveryStamp{0}; // rtn_cmd06（4B06 ACK）
    std::atomic<quint32> reportIpStamp{0};      // rtn_cmd01（重启后重新上线）
    std::atomic<quint32> transferReplyStamp{0}; // rtn_cmd05（末帧缺失列表应答）
    mutable std::mutex transferMutex;
    QVector<quint32> transferMissList;

    Device() = default;

    Device(const Device& other)
        : deviceId(other.deviceId), ip(other.ip), mask(other.mask), gateway(other.gateway),
          appPort(other.appPort), transport(other.transport), serialPort(other.serialPort),
          baudRate(other.baudRate), fwVersion(other.fwVersion),
          fwSize(other.fwSize.load()), fwCrc(other.fwCrc.load()),
          upgradeState(other.upgradeState.load()), status(other.status.load()),
          progress(other.progress), selected(other.selected),
          enterRecoveryStamp(other.enterRecoveryStamp.load()),
          reportIpStamp(other.reportIpStamp.load()),
          transferReplyStamp(other.transferReplyStamp.load())
    {
        std::lock_guard<std::mutex> lock(other.transferMutex);
        transferMissList = other.transferMissList;
    }

    Device& operator=(const Device& other)
    {
        if (this != &other) {
            deviceId = other.deviceId;
            ip = other.ip;
            mask = other.mask;
            gateway = other.gateway;
            appPort = other.appPort;
            transport = other.transport;
            serialPort = other.serialPort;
            baudRate = other.baudRate;
            fwVersion = other.fwVersion;
            fwSize.store(other.fwSize.load());
            fwCrc.store(other.fwCrc.load());
            upgradeState.store(other.upgradeState.load());
            status.store(other.status.load());
            progress = other.progress;
            selected = other.selected;
            enterRecoveryStamp.store(other.enterRecoveryStamp.load());
            reportIpStamp.store(other.reportIpStamp.load());
            transferReplyStamp.store(other.transferReplyStamp.load());
            std::lock_guard<std::mutex> lock(other.transferMutex);
            transferMissList = other.transferMissList;
        }
        return *this;
    }

    QString displayName() const
    {
        if (transport == TransportType::Udp)
            return ip.isEmpty() ? deviceId : ip;
        return serialPort.isEmpty() ? deviceId : serialPort;
    }

    bool isOnline() const { return status.load() != DeviceStatus::Offline; }
};
