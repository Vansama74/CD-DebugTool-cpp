#include "UpgradeEngine.h"

#include "protocol/iap/Crc32Mpeg2.h"
#include "protocol/iap/IapCommands.h"
#include "protocol/iap/IapFrame.h"

#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QMetaType>

UpgradeWorker::UpgradeWorker(Device* device, const QByteArray& firmware, QObject* parent)
    : QThread(parent)
    , m_device(device)
{
    // Pad to a 4-byte multiple with 0xFF, then convert to little-endian words.
    const int paddedLen = ((firmware.size() + 3) / 4) * 4;
    QByteArray padded = firmware;
    padded.append(paddedLen - firmware.size(), '\xFF');

    m_firmwareSizeBytes = static_cast<quint32>(firmware.size());
    m_firmwareWords.reserve(paddedLen / 4);
    for (int i = 0; i < paddedLen; i += 4)
        m_firmwareWords.append(IapFrame::readLe32(padded, i));

    m_totalPackets = (m_firmwareWords.size() + IapCommands::PAGE_SIZE_WORDS - 1)
                   / IapCommands::PAGE_SIZE_WORDS;
}

void UpgradeWorker::run()
{
    const QString id = m_device->deviceId;
    const QString name = m_device->displayName();

    // 1. Erase.
    emit statusChanged(id, DeviceStatus::Erasing);
    emit logMessage(QStringLiteral("[%1] 开始擦除固件 (%2 bytes)").arg(name).arg(m_firmwareSizeBytes));
    send(IapCommands::buildEraseRequest(m_firmwareSizeBytes));

    if (!waitForStatus(DeviceStatus::Transferring, 30)) {
        if (m_cancel.loadRelaxed() != 0) {
            emit logMessage(QStringLiteral("[%1] 升级已取消").arg(name));
            emit finishedSignal(id, false);
        } else {
            emit logMessage(QStringLiteral("[%1] 擦除超时").arg(name));
            emit statusChanged(id, DeviceStatus::UpgradeFailed);
            emit finishedSignal(id, false);
        }
        return;
    }

    // 2. Transfer.
    emit statusChanged(id, DeviceStatus::Transferring);
    emit logMessage(QStringLiteral("[%1] 开始传输 (%2 包, 每包 %3 word)")
                        .arg(name).arg(m_totalPackets).arg(IapCommands::PAGE_SIZE_WORDS));

    for (int pkt = 0; pkt < m_totalPackets; ++pkt) {
        while (m_paused.loadRelaxed() != 0 && m_cancel.loadRelaxed() == 0)
            msleep(100);

        if (m_cancel.loadRelaxed() != 0) {
            emit logMessage(QStringLiteral("[%1] 升级已取消").arg(name));
            emit finishedSignal(id, false);
            return;
        }

        const int offset = pkt * IapCommands::PAGE_SIZE_WORDS;
        send(IapCommands::buildTransferFrame(static_cast<quint32>(pkt + 1), m_firmwareWords, offset));

        const double percent = static_cast<double>(pkt + 1) / m_totalPackets * 100.0;
        emit progressUpdated(id, percent, pkt + 1, m_totalPackets);
        msleep(5);
    }

    // 3. Verify.
    emit statusChanged(id, DeviceStatus::Verifying);
    emit logMessage(QStringLiteral("[%1] 传输完成，等待设备确认...").arg(name));

    if (waitForStatus(DeviceStatus::UpgradeDone, 60)) {
        emit statusChanged(id, DeviceStatus::UpgradeDone);
        emit logMessage(QStringLiteral("[%1] 升级成功!").arg(name));
        emit finishedSignal(id, true);
    } else {
        if (m_cancel.loadRelaxed() != 0) {
            emit logMessage(QStringLiteral("[%1] 升级已取消").arg(name));
            emit finishedSignal(id, false);
        } else {
            emit statusChanged(id, DeviceStatus::UpgradeFailed);
            emit logMessage(QStringLiteral("[%1] 验证超时，升级失败").arg(name));
            emit finishedSignal(id, false);
        }
    }
}

bool UpgradeWorker::waitForStatus(DeviceStatus target, int timeoutSec)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutSec * 1000) {
        if (m_cancel.loadRelaxed() != 0)
            return false;
        if (m_device->status.load() == target)
            return true;
        msleep(200);
    }
    return false;
}

void UpgradeWorker::send(const QByteArray& data)
{
    if (m_sendFunc)
        m_sendFunc(data, m_device);
}

UpgradeEngine::UpgradeEngine(QObject* parent)
    : QObject(parent)
{
    // These enums cross thread boundaries via queued signals.
    qRegisterMetaType<DeviceStatus>("DeviceStatus");
    qRegisterMetaType<EngineState>("EngineState");
    // allFinished 携带聚合结果；显式注册以支持跨线程/信号侦听器。
    qRegisterMetaType<QHash<QString, bool>>("QHash<QString,bool>");
}

bool UpgradeEngine::isRunning() const
{
    return m_state != EngineState::Idle && m_state != EngineState::Done
        && m_state != EngineState::Failed;
}

QJsonObject UpgradeEngine::loadFirmware(const QString& filepath)
{
    QFile f(filepath);
    if (!f.open(QIODevice::ReadOnly))
        return QJsonObject();
    m_firmware = f.readAll();

    const qint64 size = m_firmware.size();
    const qint64 sizeWords = (size + 3) / 4;
    const qint64 totalPackets = (sizeWords + IapCommands::PAGE_SIZE_WORDS - 1)
                              / IapCommands::PAGE_SIZE_WORDS;
    const quint32 crc = Crc32Mpeg2::crc32Mpeg2(m_firmware);

    QJsonObject info;
    info.insert(QStringLiteral("path"), filepath);
    info.insert(QStringLiteral("size"), static_cast<double>(size));
    info.insert(QStringLiteral("sizeWords"), static_cast<double>(sizeWords));
    info.insert(QStringLiteral("totalPackets"), static_cast<double>(totalPackets));
    info.insert(QStringLiteral("crc"), static_cast<double>(crc));

    emit logMessage(QStringLiteral("固件已加载: %1 (%2 bytes, %3 包, CRC=0x%4)")
                        .arg(QFileInfo(filepath).fileName())
                        .arg(size)
                        .arg(totalPackets)
                        .arg(QString::number(crc, 16).toUpper().rightJustified(8, QLatin1Char('0'))));
    return info;
}

void UpgradeEngine::startUpgrade(const QVector<Device*>& devices)
{
    if (m_firmware.isEmpty()) {
        emit logMessage(QStringLiteral("错误: 未加载固件文件"));
        return;
    }
    if (devices.isEmpty()) {
        emit logMessage(QStringLiteral("错误: 未选择设备"));
        return;
    }
    if (isRunning()) {
        emit logMessage(QStringLiteral("错误: 升级正在进行中"));
        return;
    }

    m_state = EngineState::Transferring;
    m_results.clear();
    m_totalWorkers = devices.size();
    emit engineStateChanged(m_state);

    for (Device* device : devices) {
        auto* worker = new UpgradeWorker(device, m_firmware, this);
        worker->setSendFunc(m_sendFunc);

        connect(worker, &UpgradeWorker::progressUpdated, this, &UpgradeEngine::progressUpdated);
        connect(worker, &UpgradeWorker::statusChanged, this, &UpgradeEngine::deviceStatusChanged);
        connect(worker, &UpgradeWorker::logMessage, this, &UpgradeEngine::logMessage);
        connect(worker, &UpgradeWorker::finishedSignal, this, &UpgradeEngine::onWorkerFinished);
        connect(worker, &QThread::finished, this, [this, id = device->deviceId, worker]() {
            m_workers.remove(id);
            worker->deleteLater();
        });

        m_workers.insert(device->deviceId, worker);
        worker->start();
    }

    emit logMessage(QStringLiteral("开始升级 %1 台设备").arg(devices.size()));
}

void UpgradeEngine::cancelAll()
{
    for (UpgradeWorker* w : m_workers.values())
        w->cancel();
    m_state = EngineState::Idle;
    emit engineStateChanged(m_state);
    emit logMessage(QStringLiteral("所有升级已取消"));
}

void UpgradeEngine::pauseAll()
{
    for (UpgradeWorker* w : m_workers.values())
        w->pause();
    emit logMessage(QStringLiteral("升级已暂停"));
}

void UpgradeEngine::resumeAll()
{
    for (UpgradeWorker* w : m_workers.values())
        w->resume();
    emit logMessage(QStringLiteral("升级已恢复"));
}

void UpgradeEngine::waitForWorkers()
{
    for (UpgradeWorker* w : m_workers.values()) {
        if (w->isRunning()) {
            w->cancel();
            w->wait();
        }
    }
}

void UpgradeEngine::onWorkerFinished(const QString& deviceId, bool success)
{
    m_results.insert(deviceId, success);

    if (m_results.size() >= m_totalWorkers) {
        bool allOk = true;
        for (auto it = m_results.constBegin(); it != m_results.constEnd(); ++it)
            allOk = allOk && it.value();

        m_state = allOk ? EngineState::Done : EngineState::Failed;
        emit engineStateChanged(m_state);
        emit allFinished(m_results);

        int successCount = 0;
        for (auto it = m_results.constBegin(); it != m_results.constEnd(); ++it)
            if (it.value())
                ++successCount;
        emit logMessage(QStringLiteral("批量升级完成: %1 成功, %2 失败")
                            .arg(successCount)
                            .arg(m_results.size() - successCount));
    }
}
