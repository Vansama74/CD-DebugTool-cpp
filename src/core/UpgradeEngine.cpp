#include "UpgradeEngine.h"

#include "protocol/iap/Crc32Mpeg2.h"
#include "protocol/iap/IapCommands.h"
#include "protocol/iap/IapFrame.h"
#include "protocol/iap/IntelHexParser.h"

#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QMetaType>

namespace {
// —— IAP 升级时序参数 ——
// 包间隔 50ms：对齐 Java 参考工具；旧值 5ms 下设备易丢帧（UDP 无重传，
// 只能靠末帧 rtn_cmd05 缺失列表补救）。
constexpr int TRANSFER_INTERVAL_MS = 50;
// 4B06（进入升级模式）ACK 等待：主固件置 RTC backup FLAG_FORCE_UPDATE 后
// 立即回 rtn_cmd06（len=0），无应答即失败并明确提示。
constexpr int ENTER_RECOVERY_ACK_TIMEOUT_SEC = 5;
// 4B07 重启后等待设备重新上线（Bootloader 判 FLAG_FORCE_UPDATE → 跳
// Recovery，LwIP 起来后应答 4B01 广播；设备 IP 不变）。
constexpr int RECOVERY_BOOT_TIMEOUT_SEC = 30;
// 收到 4B01 重新上线应答后的稳定延时，避免 Recovery 网络栈刚起就发 4B04。
constexpr int RECOVERY_SETTLE_MS = 300;
// 每轮传输后等待 rtn_cmd05（设备只在整轮帧数到达 max_len 时应答一次）。
constexpr int TRANSFER_REPLY_TIMEOUT_SEC = 30;
// 重传最大轮数（每轮恒为 N 帧）。
constexpr int MAX_TRANSFER_ROUNDS = 10;
// 验证：每 500ms 发一次 4B03，总超时 60s（与 Java 参考一致量级）。
constexpr int VERIFY_INTERVAL_MS = 500;
constexpr int VERIFY_TIMEOUT_MS = 60 * 1000;
} // namespace

UpgradeWorker::UpgradeWorker(Device* device, const QByteArray& firmware, QObject* parent)
    : QThread(parent)
    , m_device(device)
{
    // Pad to a 4-byte multiple with 0xFF, then convert to little-endian words.
    // 填充语义与设备 Recovery 一致：4B04 擦除后未覆盖区域即 0xFF，设备
    // 按 ceil(size/4) word 计算固件 CRC（HAL_CRC_Calculate，word 流大端）
    // ——即「0xFF 填充到 4B 对齐 + crc32Mpeg2Words」，与 Java
    // CRC32_OR_MPEG_2(int[]) 相同。
    const int paddedLen = ((firmware.size() + 3) / 4) * 4;
    QByteArray padded = firmware;
    padded.append(paddedLen - firmware.size(), '\xFF');

    m_firmwareSizeBytes = static_cast<quint32>(firmware.size());
    m_firmwareWords.reserve(paddedLen / 4);
    for (int i = 0; i < paddedLen; i += 4)
        m_firmwareWords.append(IapFrame::readLe32(padded, i));
    m_firmwareCrc = Crc32Mpeg2::crc32Mpeg2Words(m_firmwareWords);

    m_totalPackets = (m_firmwareWords.size() + IapCommands::PAGE_SIZE_WORDS - 1)
                   / IapCommands::PAGE_SIZE_WORDS;
}

void UpgradeWorker::run()
{
    const QString id = m_device->deviceId;
    const QString name = m_device->displayName();

    emit logMessage(QStringLiteral("[%1] 开始升级 (%2 bytes, %3 包, CRC=0x%4)")
                        .arg(name)
                        .arg(m_firmwareSizeBytes)
                        .arg(m_totalPackets)
                        .arg(QString::number(m_firmwareCrc, 16).toUpper()
                                 .rightJustified(8, QLatin1Char('0'))));

    // 0. 进入 Recovery：主固件 4B04/4B05 是空实现（只 ACK len=0），真正的
    // 擦写 flash 只在 Recovery（STM32F407-Recovery/Drivers/BSP/Cmd/cmd.c）。
    // 必须先 4B06（主固件置 FLAG_FORCE_UPDATE 并 ACK，不重启）→ 4B07 重启，
    // Bootloader 见标志跳 Recovery（条件 A）。
    if (!enterRecoveryPhase(id, name))
        return;

    // 1. Erase（4B04：Recovery 真正擦除 Sector6 起 N 扇区，rtn_cmd04 带结果码）。
    if (!erasePhase(id, name))
        return;

    // 2. Transfer（4B05：整轮发送 + rtn_cmd05 缺失列表重传）。
    if (!transferPhase(id, name))
        return;

    // 3. Verify（4B03 轮询，update_sta==0 即成功；比对本机/设备 CRC 与 size）。
    if (!verifyPhase(id, name))
        return;

    // 4. 重启回主固件（Recovery 4B07 ACK 后 NVIC 复位；Bootloader 条件 D
    // 校验通过后跳 0x08040000 新固件）。
    send(IapCommands::buildRebootRequest());
    emit statusChanged(id, DeviceStatus::UpgradeDone);
    emit logMessage(QStringLiteral("[%1] 升级成功! 已发送重启请求，设备将启动新固件").arg(name));
    emit finishedSignal(id, true);
}

bool UpgradeWorker::enterRecoveryPhase(const QString& id, const QString& name)
{
    emit logMessage(QStringLiteral("[%1] 发送进入升级模式请求 (4B06)...").arg(name));
    const quint32 base = m_device->enterRecoveryStamp.load();
    send(IapCommands::buildEnterRecoveryRequest());
    if (!waitForStamp(m_device->enterRecoveryStamp, base, ENTER_RECOVERY_ACK_TIMEOUT_SEC)) {
        if (m_cancel.loadRelaxed() == 0)
            failPhase(id, name, QStringLiteral("[%1] 设备未响应进入升级模式 (4B06 无 ACK)，升级中止")
                                    .arg(name));
        else
            failPhase(id, name, QStringLiteral("[%1] 升级已取消").arg(name));
        return false;
    }

    emit logMessage(QStringLiteral("[%1] 已确认进入升级模式，发送重启请求 (4B07)...").arg(name));
    send(IapCommands::buildRebootRequest());

    // 等设备重启后以 4B01 广播确认重新上线（Recovery 应答 rtn_cmd01 广播）。
    // 串口模式无广播通道，跳过上线等待，仅做固定延时（后续 4B04 超时会给出
    // 明确失败提示——Recovery 的 IAP 仅走 UDP）。
    if (m_broadcastFunc) {
        const quint32 onlineBase = m_device->reportIpStamp.load();
        QElapsedTimer timer;
        timer.start();
        bool reappeared = false;
        while (timer.elapsed() < RECOVERY_BOOT_TIMEOUT_SEC * 1000) {
            if (m_cancel.loadRelaxed() != 0) {
                failPhase(id, name, QStringLiteral("[%1] 升级已取消").arg(name));
                return false;
            }
            m_broadcastFunc(IapCommands::buildReportIpRequest());
            if (m_device->reportIpStamp.load() != onlineBase) {
                reappeared = true;
                break;
            }
            msleep(1000);
        }
        if (!reappeared) {
            failPhase(id, name,
                      QStringLiteral("[%1] 设备重启后未重新上线（%2s 内未收到 4B01 应答），升级中止")
                          .arg(name)
                          .arg(RECOVERY_BOOT_TIMEOUT_SEC));
            return false;
        }
        emit logMessage(QStringLiteral("[%1] 设备已重新上线（Recovery），等待网络稳定...").arg(name));
        msleep(RECOVERY_SETTLE_MS);
    } else {
        emit logMessage(QStringLiteral("[%1] 串口模式无广播通道，等待设备重启 (%2s)...")
                            .arg(name)
                            .arg(RECOVERY_BOOT_TIMEOUT_SEC));
        msleep(RECOVERY_BOOT_TIMEOUT_SEC * 1000 / 2);
    }
    return true;
}

bool UpgradeWorker::erasePhase(const QString& id, const QString& name)
{
    emit statusChanged(id, DeviceStatus::Erasing);
    emit logMessage(QStringLiteral("[%1] 开始擦除固件 (%2 bytes)")
                        .arg(name)
                        .arg(m_firmwareSizeBytes));
    send(IapCommands::buildEraseRequest(m_firmwareSizeBytes));

    // Recovery rtn_cmd04 载荷[0]==0 → DeviceManager 置 Transferring；
    // 载荷非 0 / 空 ACK（如串口连到主固件的空实现 4B04）→ UpgradeFailed。
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 30 * 1000) {
        if (m_cancel.loadRelaxed() != 0) {
            failPhase(id, name, QStringLiteral("[%1] 升级已取消").arg(name));
            return false;
        }
        const DeviceStatus st = m_device->status.load();
        if (st == DeviceStatus::Transferring)
            return true;
        if (st == DeviceStatus::UpgradeFailed) {
            failPhase(id, name,
                      QStringLiteral("[%1] 擦除失败（设备拒绝 4B04；串口模式 Recovery 不响应）")
                          .arg(name));
            return false;
        }
        msleep(200);
    }
    failPhase(id, name, QStringLiteral("[%1] 擦除超时（Recovery 未确认 4B04）").arg(name));
    return false;
}

QVector<quint32> UpgradeWorker::buildRoundSeqs(const QVector<quint32>& missing) const
{
    QVector<quint32> seqs;
    seqs.reserve(m_totalPackets);
    // 优先重传缺失帧（索引 0-based → seq = idx + 1）。
    for (quint32 idx : missing) {
        if (idx + 1 <= static_cast<quint32>(m_totalPackets))
            seqs.append(idx + 1);
    }
    // 补齐到整轮 N 帧：设备只在「一轮内收到帧数达到 max_len」时应答
    // rtn_cmd05（Recovery cmd.c 的 frame_cnt 逻辑），只重传缺失帧不会触发
    // 新应答。重复帧写同地址、置同 bitmap 位，幂等无害。
    for (int i = 0; seqs.size() < m_totalPackets && i < m_totalPackets; ++i)
        seqs.append(static_cast<quint32>(i + 1));
    return seqs;
}

bool UpgradeWorker::transferPhase(const QString& id, const QString& name)
{
    emit statusChanged(id, DeviceStatus::Transferring);
    emit logMessage(QStringLiteral("[%1] 开始传输 (%2 包, 每包 %3 word, 间隔 %4ms)")
                        .arg(name)
                        .arg(m_totalPackets)
                        .arg(IapCommands::PAGE_SIZE_WORDS)
                        .arg(TRANSFER_INTERVAL_MS));

    QVector<quint32> missing;
    for (int round = 0; round < MAX_TRANSFER_ROUNDS; ++round) {
        const QVector<quint32> seqs = buildRoundSeqs(missing);
        if (round == 0) {
            emit logMessage(QStringLiteral("[%1] 第 1 轮发送 %2 帧").arg(name).arg(seqs.size()));
        } else {
            emit logMessage(QStringLiteral("[%1] 第 %2 轮：重传 %3 个缺失帧 + 补齐整轮（共 %4 帧）")
                                .arg(name)
                                .arg(round + 1)
                                .arg(missing.size())
                                .arg(seqs.size()));
        }

        const quint32 replyBase = m_device->transferReplyStamp.load();
        int sent = 0;
        for (quint32 seq : seqs) {
            while (m_paused.loadRelaxed() != 0 && m_cancel.loadRelaxed() == 0)
                msleep(100);
            if (m_cancel.loadRelaxed() != 0) {
                failPhase(id, name, QStringLiteral("[%1] 升级已取消").arg(name));
                return false;
            }

            const int offset = static_cast<int>(seq - 1) * IapCommands::PAGE_SIZE_WORDS;
            send(IapCommands::buildTransferFrame(seq, m_firmwareWords, offset));
            ++sent;

            // 进度只随首轮推进；重传轮保持 100%。
            if (round == 0) {
                const double percent = static_cast<double>(sent) / m_totalPackets * 100.0;
                emit progressUpdated(id, percent, sent, m_totalPackets);
            }
            msleep(TRANSFER_INTERVAL_MS);
        }

        // 等设备末帧应答（rtn_cmd05，seq=0，载荷=缺失帧索引列表）。
        if (!waitForStamp(m_device->transferReplyStamp, replyBase, TRANSFER_REPLY_TIMEOUT_SEC)) {
            if (m_cancel.loadRelaxed() != 0) {
                failPhase(id, name, QStringLiteral("[%1] 升级已取消").arg(name));
                return false;
            }
            emit logMessage(QStringLiteral("[%1] 第 %2 轮未收到设备应答，重发整轮...")
                                .arg(name)
                                .arg(round + 1));
            missing.clear();
            continue;
        }

        missing = takeMissList();
        if (missing.isEmpty()) {
            emit logMessage(QStringLiteral("[%1] 设备确认全部 %2 帧接收完成").arg(name)
                                .arg(m_totalPackets));
            return true;
        }
    }

    failPhase(id, name, QStringLiteral("[%1] 传输失败：重传 %2 轮后仍有缺失帧")
                            .arg(name)
                            .arg(MAX_TRANSFER_ROUNDS));
    return false;
}

bool UpgradeWorker::verifyPhase(const QString& id, const QString& name)
{
    emit statusChanged(id, DeviceStatus::Verifying);
    emit logMessage(QStringLiteral("[%1] 传输完成，轮询设备升级状态 (4B03 每 %2ms，最长 %3s)...")
                        .arg(name)
                        .arg(VERIFY_INTERVAL_MS)
                        .arg(VERIFY_TIMEOUT_MS / 1000));

    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < VERIFY_TIMEOUT_MS) {
        if (m_cancel.loadRelaxed() != 0) {
            failPhase(id, name, QStringLiteral("[%1] 升级已取消").arg(name));
            return false;
        }
        send(IapCommands::buildQueryStatusRequest());

        // rtn_cmd03 update_sta==0 → DeviceManager 置 UpgradeDone。
        QElapsedTimer poll;
        poll.start();
        while (poll.elapsed() < VERIFY_INTERVAL_MS) {
            const DeviceStatus st = m_device->status.load();
            if (st == DeviceStatus::UpgradeDone) {
                // 设备 update_sta==0 即成功。另用设备返回的 size/crc 与本机
                // 计算值（0xFF 填充 + crc32Mpeg2Words）比对显示——不一致仅告警
                // （设备侧 CRC 由 Recovery 写后自算，update_sta==0 已含自检）。
                const quint32 devSize = m_device->fwSize.load();
                const quint32 devCrc = m_device->fwCrc.load();
                const bool sizeOk = (devSize == m_firmwareSizeBytes);
                const bool crcOk = (devCrc == m_firmwareCrc);
                emit logMessage(
                    QStringLiteral("[%1] 设备上报: size=%2 crc=0x%3 | 本地固件: size=%4 crc=0x%5 → %6")
                        .arg(name)
                        .arg(devSize)
                        .arg(QString::number(devCrc, 16).toUpper().rightJustified(8, QLatin1Char('0')))
                        .arg(m_firmwareSizeBytes)
                        .arg(QString::number(m_firmwareCrc, 16).toUpper()
                                 .rightJustified(8, QLatin1Char('0')))
                        .arg(sizeOk && crcOk ? QStringLiteral("一致")
                                             : QStringLiteral("不一致(告警)")));
                return true;
            }
            if (st == DeviceStatus::UpgradeFailed) {
                failPhase(id, name, QStringLiteral("[%1] 设备报告升级失败").arg(name));
                return false;
            }
            msleep(50);
        }
    }

    failPhase(id, name, QStringLiteral("[%1] 验证超时（%2s 内 update_sta 未变为 0），升级失败")
                            .arg(name)
                            .arg(VERIFY_TIMEOUT_MS / 1000));
    return false;
}

void UpgradeWorker::failPhase(const QString& id, const QString& name, const QString& reason)
{
    if (m_cancel.loadRelaxed() != 0)
        emit logMessage(QStringLiteral("[%1] 升级已取消").arg(name));
    else {
        emit statusChanged(id, DeviceStatus::UpgradeFailed);
        emit logMessage(reason);
    }
    emit finishedSignal(id, false);
}

bool UpgradeWorker::waitForStamp(const std::atomic<quint32>& stamp, quint32 baseline,
                                 int timeoutSec)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutSec * 1000) {
        if (m_cancel.loadRelaxed() != 0)
            return false;
        if (stamp.load() != baseline)
            return true;
        msleep(100);
    }
    return false;
}

QVector<quint32> UpgradeWorker::takeMissList() const
{
    std::lock_guard<std::mutex> lock(m_device->transferMutex);
    return m_device->transferMissList;
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
    const QByteArray raw = f.readAll();
    f.close();

    // .hex → Intel HEX 解析为字节流（与 .bin 等价）；其余（.bin/.img）原样。
    QByteArray bytes;
    const QString suffix = QFileInfo(filepath).suffix().toLower();
    if (suffix == QLatin1String("hex")) {
        QString err;
        if (!IntelHexParser::parse(raw, &bytes, &err)) {
            emit logMessage(QStringLiteral("Intel HEX 解析失败 (%1): %2")
                                .arg(QFileInfo(filepath).fileName(), err));
            return QJsonObject();
        }
    } else {
        bytes = raw;
    }
    if (bytes.isEmpty()) {
        emit logMessage(QStringLiteral("固件文件为空: %1").arg(QFileInfo(filepath).fileName()));
        return QJsonObject();
    }
    m_firmware = bytes;

    // 固件 CRC：0xFF 填充到 4B 对齐 + crc32Mpeg2Words（逐 word 大端 MPEG-2）。
    // 与设备 Recovery 写入后 HAL_CRC_Calculate（word 流大端）及 Java
    // CRC32_OR_MPEG_2(int[]) 一致；旧实现逐字节 crc32Mpeg2 且不填充，与
    // 设备 CRC 恒不等，4B03 比对必失败。
    const int paddedLen = ((bytes.size() + 3) / 4) * 4;
    QByteArray padded = bytes;
    padded.append(paddedLen - bytes.size(), '\xFF');
    QVector<quint32> words;
    words.reserve(paddedLen / 4);
    for (int i = 0; i < paddedLen; i += 4)
        words.append(IapFrame::readLe32(padded, i));
    const quint32 crc = Crc32Mpeg2::crc32Mpeg2Words(words);

    const qint64 size = bytes.size();
    const qint64 sizeWords = words.size();
    const qint64 totalPackets = (sizeWords + IapCommands::PAGE_SIZE_WORDS - 1)
                              / IapCommands::PAGE_SIZE_WORDS;

    QJsonObject info;
    info.insert(QStringLiteral("path"), filepath);
    info.insert(QStringLiteral("size"), static_cast<double>(size));
    info.insert(QStringLiteral("sizeWords"), static_cast<double>(sizeWords));
    info.insert(QStringLiteral("totalPackets"), static_cast<double>(totalPackets));
    info.insert(QStringLiteral("crc"), static_cast<double>(crc));

    emit logMessage(QStringLiteral("固件已加载: %1 (%2 bytes, %3 words, %4 包, CRC=0x%5)")
                        .arg(QFileInfo(filepath).fileName())
                        .arg(size)
                        .arg(sizeWords)
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
        worker->setBroadcastFunc(m_broadcastFunc);

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