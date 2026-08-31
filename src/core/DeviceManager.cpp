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
        existing.fwSize.store(device.fwSize.load());
        existing.fwCrc.store(device.fwCrc.load());
        existing.upgradeState.store(device.upgradeState.load());
        existing.status.store(device.status.load());
        existing.progress = device.progress;
        existing.selected = device.selected;
        // 邮箱字段随 update 一起复制（worker 线程轮询这些计数）。
        existing.enterRecoveryStamp.store(device.enterRecoveryStamp.load());
        existing.reportIpStamp.store(device.reportIpStamp.load());
        existing.transferReplyStamp.store(device.transferReplyStamp.load());
        {
            std::lock_guard<std::mutex> lock(existing.transferMutex);
            existing.transferMissList = device.transferMissList;
        }
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
            // cmd01 应答由设备广播（源地址 255.255.255.255，见设备端
            // cmd_SendReData：rtn_cmd01/rtn_cmd02 走广播）。源地址不唯一，
            // 须以载荷内上报的 IP 作为设备标识；否则多设备回复会互相覆盖、
            // 后续单播查询（按真实源 IP）也找不到已注册设备。
            const bool ipUsable = !info.ip.isEmpty() &&
                                  info.ip != QStringLiteral("0.0.0.0");
            const QString id = (transport == TransportType::Udp && ipUsable)
                                   ? info.ip
                                   : srcId;
            Device* existing = getDevice(id);
            Device d;
            if (existing) {
                d = *existing; // preserve firmware info / selection on update
            } else {
                d.deviceId = id;
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
            m_lastSeen[id] = nowMs();
            // 4B01 应答 = 设备重新上线证据（升级前置重启后 Recovery 重新
            // 广播应答）。worker 以该计数变化判定「重启完成、可继续 4B04」。
            if (Device* online = getDevice(id))
                online->reportIpStamp.fetch_add(1);
        }
    } else if (cmd == IapCommands::RESP_QUERY_STATUS) {
        if (dev) {
            const StatusInfo info = IapCommands::parseStatusResponse(payload);
            if (info.valid) {
                dev->fwVersion = info.version;
                dev->fwSize.store(info.fwSize);
                dev->fwCrc.store(info.fwCrc);
                dev->upgradeState.store(info.upgradeState);

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
    } else if (cmd == IapCommands::RESP_SET_IP) {
        // 4B02 应答：主固件 rtn_cmd02 带 1 word 结果码（0=成功）、Recovery
        // 空载荷 ACK。应答与 rtn_cmd01 一样经广播回（cmd_SendReData 对
        // rtn_cmd01/02 置 255.255.255.255），srcId 为设备真实源 IP。
        if (dev) {
            const bool ok = IapCommands::parseSetIpResponse(payload);
            emit setIpAck(srcId, ok);
            emit logMessage(ok ? QStringLiteral("[%1] 配置下发成功（设备重启后生效）")
                                   .arg(dev->displayName())
                               : QStringLiteral("[%1] 配置下发失败").arg(dev->displayName()));
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
            // 末帧应答邮箱：worker 轮询 transferReplyStamp 判定应答到达并读取
            // 缺失帧索引列表（0-based，空=全部收到）。设备只在「一轮收到的
            // 帧数达到 max_len」时应答（Recovery cmd.c cmd_SendUpgradePackage_05
            // 的 frame_cnt 逻辑），故 worker 以整轮 N 帧为单位发送。
            {
                std::lock_guard<std::mutex> lock(dev->transferMutex);
                dev->transferMissList = badPages;
            }
            dev->transferReplyStamp.fetch_add(1);
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
    } else if (cmd == IapCommands::RESP_ENTER_RECOVERY) {
        // 主固件 4B06 处理：置 RTC backup FLAG_FORCE_UPDATE 后 ACK len=0
        // （app_iap_cmd.c cmd_EnterRecoveryMode_06），不重启。worker 以该
        // 计数确认「设备已确认进入升级模式」，随后才发 4B07 重启。
        if (dev) {
            dev->enterRecoveryStamp.fetch_add(1);
            emit logMessage(QStringLiteral("[%1] 设备已确认进入升级模式").arg(dev->displayName()));
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
