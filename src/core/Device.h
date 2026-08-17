#pragma once
#include <QString>
#include <QtGlobal>

#include <atomic>

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
    quint16 appPort = 0;
    TransportType transport = TransportType::Udp;
    QString serialPort;
    int baudRate = 115200;
    QString fwVersion;
    quint32 fwSize = 0;
    quint32 fwCrc = 0;
    int upgradeState = -1;
    std::atomic<DeviceStatus> status{DeviceStatus::Offline};
    int progress = 0;
    bool selected = true;

    Device() = default;

    Device(const Device& other)
        : deviceId(other.deviceId), ip(other.ip), mask(other.mask), gateway(other.gateway),
          appPort(other.appPort), transport(other.transport), serialPort(other.serialPort),
          baudRate(other.baudRate), fwVersion(other.fwVersion), fwSize(other.fwSize),
          fwCrc(other.fwCrc), upgradeState(other.upgradeState),
          status(other.status.load()), progress(other.progress), selected(other.selected) {}

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
            fwSize = other.fwSize;
            fwCrc = other.fwCrc;
            upgradeState = other.upgradeState;
            status.store(other.status.load());
            progress = other.progress;
            selected = other.selected;
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
