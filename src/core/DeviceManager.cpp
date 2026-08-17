#include "DeviceManager.h"

#include "protocol/iap/IapCommands.h"

#include <QDateTime>
#include <QStringList>

namespace {
constexpr qint64 TIMEOUT_MS = 30 * 1000;
}

DeviceManager::DeviceManager(QObject* parent)
    : QObject(parent)
{
    m_timer.setInterval(5000);
    connect(&m_timer, &QTimer::timeout, this, &DeviceManager::checkTimeouts);
    m_timer.start();
}

qint64 DeviceManager::nowMs() const
{
    return QDateTime::currentMSecsSinceEpoch();
}

QVector<Device*> DeviceManager::devices()
{
    QVector<Device*> result;
    result.reserve(m_devices.size());
    for (auto it = m_devices.begin(); it != m_devices.end(); ++it)
        result.append(&it.value());
    return result;
}

Device* DeviceManager::getDevice(const QString& id)
{
    auto it = m_devices.find(id);
    return it == m_devices.end() ? nullptr : &it.value();
}

QVector<Device*> DeviceManager::getOnlineDevices()
{
    QVector<Device*> result;
    for (auto it = m_devices.begin(); it != m_devices.end(); ++it) {
        if (it->isOnline())
            result.append(&it.value());
    }
    return result;
}

QVector<Device*> DeviceManager::getSelectedDevices()
{
    QVector<Device*> result;
    for (auto it = m_devices.begin(); it != m_devices.end(); ++it) {
        if (it->selected && it->isOnline())
            result.append(&it.value());
    }
    return result;
}

void DeviceManager::addOrUpdateDevice(const Device& device)
{
    auto it = m_devices.find(device.deviceId);
    if (it != m_devices.end()) {
        Device& existing = it.value();
        existing.ip = device.ip;
        existing.mask = device.mask;
        existing.gateway = device.gateway;
        existing.appPort = device.appPort;
        existing.transport = device.transport;
        existing.serialPort = device.serialPort;
        existing.baudRate = device.baudRate;
        existing.fwVersion = device.fwVersion;
        existing.fwSize = device.fwSize;
        existing.fwCrc = device.fwCrc;
        existing.upgradeState = device.upgradeState;
        existing.status.store(device.status.load());
        existing.progress = device.progress;
        existing.selected = device.selected;
        m_lastSeen[device.deviceId] = nowMs();
        emit deviceUpdated(device.deviceId);
    } else {
        m_devices.insert(device.deviceId, device);
        m_lastSeen[device.deviceId] = nowMs();
        emit deviceAdded(device.deviceId);
        emit logMessage(QStringLiteral("发现设备: %1").arg(device.displayName()));
    }
}

void DeviceManager::handleFrame(const IapFrame::ParsedFrame& parsed, const QString& srcId,
                                TransportType transport)
{
    const quint32 cmd = parsed.cmd;
    const QVector<quint32>& payload = parsed.payloadWords;
    Device* dev = getDevice(srcId);

    if (cmd == IapCommands::RESP_REPORT_IP) {
        const ReportIpInfo info = IapCommands::parseReportIpResponse(payload);
        if (info.valid) {
            Device d;
            if (dev) {
                d = *dev; // preserve firmware info / selection on update
            } else {
                d.deviceId = srcId;
                d.transport = transport;
                if (transport == TransportType::Serial)
                    d.serialPort = srcId;
            }
            d.ip = info.ip;
            d.mask = info.mask;
            d.gateway = info.gateway;
            d.appPort = info.appPort;
            d.status.store(DeviceStatus::Online);
            addOrUpdateDevice(d);
        }
    } else if (cmd == IapCommands::RESP_QUERY_STATUS) {
        if (dev) {
            const StatusInfo info = IapCommands::parseStatusResponse(payload);
            if (info.valid) {
                dev->fwVersion = info.version;
                dev->fwSize = info.fwSize;
                dev->fwCrc = info.fwCrc;
                dev->upgradeState = info.upgradeState;

                if (dev->status.load() != DeviceStatus::Erasing &&
                    dev->status.load() != DeviceStatus::Transferring) {
                    // Bug fix #1: a status response reporting SUCCESS means the
                    // device finished upgrading (so the verify step completes),
                    // NOT merely "online".
                    if (info.upgradeState == static_cast<int>(IapCommands::UPGRADE_STATE_SUCCESS))
                        dev->status.store(DeviceStatus::UpgradeDone);
                    else
                        dev->status.store(DeviceStatus::Online);
                }

                emit deviceUpdated(srcId);
                emit logMessage(QStringLiteral("[%1] 固件状态: 版本=%2, 大小=%3字, 状态=%4")
                                    .arg(dev->displayName(), info.version)
                                    .arg(info.fwSize)
                                    .arg(deviceStatusLabel(dev->status.load())));
            }
        }
    } else if (cmd == IapCommands::RESP_ERASE_FW) {
        if (dev) {
            const bool success = IapCommands::parseEraseResponse(payload);
            if (success) {
                dev->status.store(DeviceStatus::Transferring);
                emit logMessage(QStringLiteral("[%1] 擦除完成").arg(dev->displayName()));
            } else {
                dev->status.store(DeviceStatus::UpgradeFailed);
                emit logMessage(QStringLiteral("[%1] 擦除失败!").arg(dev->displayName()));
            }
            emit deviceUpdated(srcId);
        }
    } else if (cmd == IapCommands::RESP_TRANSFER_FW) {
        if (dev) {
            const QVector<quint32> badPages = IapCommands::parseTransferResponse(payload);
            if (badPages.isEmpty()) {
                // Bug fix #1b: an empty bad-page list means transfer complete ->
                // move to Verifying (NOT UpgradeDone).
                dev->status.store(DeviceStatus::Verifying);
                emit logMessage(QStringLiteral("[%1] 传输完成，等待验证").arg(dev->displayName()));
            } else {
                QStringList parts;
                for (quint32 p : badPages)
                    parts << QString::number(p);
                emit logMessage(QStringLiteral("[%1] 需要重传 %2 个包: %3")
                                    .arg(dev->displayName())
                                    .arg(badPages.size())
                                    .arg(parts.join(QLatin1Char(','))));
            }
            emit deviceUpdated(srcId);
        }
    }

    m_lastSeen[srcId] = nowMs();
}

void DeviceManager::markOffline(const QString& id)
{
    Device* dev = getDevice(id);
    if (dev) {
        dev->status.store(DeviceStatus::Offline);
        emit deviceUpdated(id);
    }
}

void DeviceManager::removeDevice(const QString& id)
{
    if (m_devices.remove(id) > 0) {
        m_lastSeen.remove(id);
        emit deviceRemoved(id);
    }
}

void DeviceManager::clearAll()
{
    const QStringList ids = m_devices.keys();
    for (const QString& id : ids)
        removeDevice(id);
}

void DeviceManager::checkTimeouts()
{
    const qint64 now = nowMs();
    for (auto it = m_lastSeen.begin(); it != m_lastSeen.end(); ++it) {
        if (now - it.value() <= TIMEOUT_MS)
            continue;
        Device* dev = getDevice(it.key());
        if (dev && dev->status.load() == DeviceStatus::Online) {
            dev->status.store(DeviceStatus::Offline);
            emit deviceUpdated(it.key());
        }
    }
}
