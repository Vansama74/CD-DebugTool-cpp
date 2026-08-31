#include "IapPage.h"

#include "core/DeviceManager.h"
#include "protocol/iap/IapCommands.h"
#include "transport/SerialTransport.h"
#include "transport/UdpTransport.h"
#include "ui/pages/IapDevicePanel.h"
#include "ui/pages/IapUpgradePanel.h"
#include "ui/widgets/ConnectConfigPanel.h"
#include "ui/widgets/LogPanel.h"

#include <QHostAddress>
#include <QJsonObject>
#include <QMessageBox>
#include <QSplitter>
#include <QVBoxLayout>

IapPage::IapPage(QWidget* parent)
    : IProtocolPage(parent)
{
    m_deviceMgr = new DeviceManager(this);
    m_engine = new UpgradeEngine(this);
    m_serial = new SerialTransport(); // unparented: lives in its own thread
    m_udp = new UdpTransport();       // unparented: lives in its own thread

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    m_connect = new ConnectConfigPanel(ProtocolConnectMode::UdpAndSerial, this);
    layout->addWidget(m_connect);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    m_devicePanel = new IapDevicePanel(splitter);
    m_upgradePanel = new IapUpgradePanel(splitter);
    splitter->addWidget(m_devicePanel);
    splitter->addWidget(m_upgradePanel);
    splitter->setSizes({350, 650});
    layout->addWidget(splitter, 1);

    // ConnectConfigPanel -> transports.
    connect(m_connect, &ConnectConfigPanel::portOpened, this, &IapPage::onSerialOpened);
    connect(m_connect, &ConnectConfigPanel::portClosed, this, &IapPage::onSerialClosed);
    connect(m_connect, &ConnectConfigPanel::transportChanged, this, &IapPage::onTransportChanged);

    // Device panel -> page.
    connect(m_devicePanel, &IapDevicePanel::scanRequested, this, &IapPage::onScan);
    connect(m_devicePanel, &IapDevicePanel::selectAllRequested, this, &IapPage::onSelectAll);
    connect(m_devicePanel, &IapDevicePanel::selectionChanged, this, &IapPage::onSelectionChanged);
    connect(m_devicePanel, &IapDevicePanel::deviceSelected, this, &IapPage::onDeviceSelected);
    connect(m_devicePanel, &IapDevicePanel::configApplyRequested, this, &IapPage::onConfigApply);
    connect(m_devicePanel, &IapDevicePanel::statusQueryRequested, this, &IapPage::onQueryStatus);

    // Upgrade panel -> page / engine.
    connect(m_upgradePanel, &IapUpgradePanel::firmwareSelected, this, &IapPage::onFirmwareSelected);
    connect(m_upgradePanel, &IapUpgradePanel::startUpgrade, this, &IapPage::onStartUpgrade);
    connect(m_upgradePanel, &IapUpgradePanel::cancelUpgrade, this, &IapPage::onCancelUpgrade);
    connect(m_upgradePanel, &IapUpgradePanel::pauseUpgrade, this,
            [this]() { m_engine->pauseAll(); });
    connect(m_upgradePanel, &IapUpgradePanel::resumeUpgrade, this,
            [this]() { m_engine->resumeAll(); });
    connect(m_upgradePanel, &IapUpgradePanel::rebootRequested, this, &IapPage::onReboot);
    connect(m_upgradePanel, &IapUpgradePanel::recoveryRequested, this, &IapPage::onRecovery);

    // Device manager -> page.
    connect(m_deviceMgr, &DeviceManager::deviceAdded, this, &IapPage::onDeviceAdded);
    connect(m_deviceMgr, &DeviceManager::deviceUpdated, this, &IapPage::onDeviceUpdated);
    connect(m_deviceMgr, &DeviceManager::deviceRemoved, this, &IapPage::onDeviceRemoved);
    connect(m_deviceMgr, &DeviceManager::logMessage, this,
            [this](const QString& msg) { appendLog(msg, QStringLiteral("INFO")); });
    connect(m_deviceMgr, &DeviceManager::setIpAck, this, &IapPage::onSetIpAck);

    // Upgrade engine -> page.
    connect(m_engine, &UpgradeEngine::logMessage, this,
            [this](const QString& msg) { appendLog(msg, QStringLiteral("INFO")); });
    connect(m_engine, &UpgradeEngine::progressUpdated, this, &IapPage::onEngineProgress);
    connect(m_engine, &UpgradeEngine::deviceStatusChanged, this, &IapPage::onEngineStatus);
    connect(m_engine, &UpgradeEngine::engineStateChanged, this, &IapPage::onEngineState);

    // Transports -> page.
    connect(m_udp, &UdpTransport::frameReceived, this, &IapPage::onFrameReceived);
    connect(m_udp, &UdpTransport::errorOccurred, this, &IapPage::onTransportError);
    connect(m_serial, &SerialTransport::bytesReceived, this, &IapPage::onBytesReceived);
    connect(m_serial, &SerialTransport::connected, this,
            [this](const QString& port, int baud) {
                appendLog(QStringLiteral("IAP 串口 %1 @ %2 已打开").arg(port).arg(baud),
                          QStringLiteral("SUCCESS"));
            });
    connect(m_serial, &SerialTransport::disconnected, this,
            [this]() { appendLog(QStringLiteral("IAP 串口已关闭"), QStringLiteral("INFO")); });
    // 串口意外断开（拔线等）时回写连接面板状态，避免“假连接”。
    connect(m_serial, &SerialTransport::disconnected,
            m_connect, &ConnectConfigPanel::onTransportDisconnected);
    connect(m_serial, &SerialTransport::errorOccurred, this, &IapPage::onTransportError);
}

IapPage::~IapPage()
{
    // 断开引擎信号：析构期 cancelAll() 的同步信号回调不能再触达本页
    // （共享 LogPanel 可能已先析构）。
    if (m_engine)
        m_engine->disconnect(this);
    deactivate();
    delete m_serial;
    m_serial = nullptr;
    delete m_udp;
    m_udp = nullptr;
}

QString IapPage::key() const { return QStringLiteral("iap"); }
QString IapPage::fullName() const { return QStringLiteral("IAP 远程升级"); }

void IapPage::activate()
{
    if (m_serial)
        m_serial->start();
    if (m_udp)
        m_udp->start();
    if (m_transport == TransportType::Udp)
        bindUdp();
}

void IapPage::deactivate()
{
    // 先取消并等待所有升级 worker 结束，避免“升级中切页/关窗 →
    // QThread destroyed while running”崩溃。
    if (m_engine) {
        m_engine->cancelAll();
        m_engine->waitForWorkers();
    }
    if (m_connect && m_connect->isSerialOpen())
        m_connect->setSerialOpenState(false);
    if (m_serial)
        m_serial->stop();
    if (m_udp)
        m_udp->stop();
}

void IapPage::bindUdp()
{
    if (!m_udp)
        return;
    m_udp->start();
    // bindIp 传空：UdpTransport 恒通配绑定 0.0.0.0（设备 4B01 应答是广播帧，
    // 绑定具体网卡 IP 会收不到），广播发送覆盖本机所有网卡的所有广播地址
    // （见 UdpTransport::doSendBroadcast / doBind 注释）。
    m_udp->bind(static_cast<quint16>(m_connect->getListenPort()), QString());
}

void IapPage::onSerialOpened(const QString& port, int baud)
{
    if (m_serial)
        m_serial->open(port, baud);
}

void IapPage::onSerialClosed()
{
    if (m_serial)
        m_serial->close();
}

void IapPage::onTransportChanged(int idx)
{
    m_transport = (idx == 1) ? TransportType::Serial : TransportType::Udp;

    // Switching transport: stop UDP and clear the (stale) device list.
    if (m_udp)
        m_udp->stop();
    if (m_deviceMgr)
        m_deviceMgr->clearAll();
    m_rxBuffer.clear();
    m_currentDeviceId.clear();

    if (m_transport == TransportType::Udp)
        bindUdp();
}

void IapPage::onScan()
{
    const QByteArray frame = IapCommands::buildReportIpRequest();

    if (m_transport == TransportType::Udp) {
        bindUdp();
        if (m_udp)
            m_udp->sendBroadcast(frame, static_cast<quint16>(m_connect->getDevicePort()));
        appendLog(QStringLiteral("已发送UDP广播扫描请求"), QStringLiteral("CMD"));
    } else {
        if (!m_connect || !m_connect->isSerialOpen()) {
            appendLog(QStringLiteral("请先在连接配置中打开串口"), QStringLiteral("ERROR"));
            return;
        }
        if (m_serial)
            m_serial->send(frame);
        appendLog(QStringLiteral("已通过串口发送扫描请求"), QStringLiteral("CMD"));
    }
}

void IapPage::onSelectAll()
{
    for (Device* d : m_deviceMgr->devices()) {
        d->selected = true;
        m_devicePanel->updateDevice(d);
    }
    appendLog(QStringLiteral("已全选所有设备"), QStringLiteral("INFO"));
}

void IapPage::onSelectionChanged(const QString& id, bool selected)
{
    Device* d = m_deviceMgr->getDevice(id);
    if (d)
        d->selected = selected;
}

void IapPage::onDeviceSelected(const QString& id)
{
    m_currentDeviceId = id;
    Device* d = m_deviceMgr->getDevice(id);
    if (d)
        m_devicePanel->showDeviceConfig(d);
}

void IapPage::onDeviceAdded(const QString& id)
{
    Device* d = m_deviceMgr->getDevice(id);
    if (d)
        m_devicePanel->addDevice(d);
}

void IapPage::onDeviceUpdated(const QString& id)
{
    Device* d = m_deviceMgr->getDevice(id);
    if (d)
        m_devicePanel->updateDevice(d);
    // 配置区刷新（4B01 重新上线 / 4B03 固件状态应答）：只更新展示标签，
    // 不清用户正在编辑的输入框。
    if (d && id == m_currentDeviceId)
        m_devicePanel->updateDeviceInfo(d);
}

void IapPage::onDeviceRemoved(const QString& id)
{
    m_devicePanel->removeDevice(id);
}

void IapPage::onFirmwareSelected(const QString& path)
{
    const QJsonObject info = m_engine->loadFirmware(path);
    if (!info.isEmpty())
        m_upgradePanel->updateFirmwareInfo(info);
}

void IapPage::onStartUpgrade()
{
    const QVector<Device*> devices = m_deviceMgr->getSelectedDevices();
    if (devices.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先扫描并选择设备"));
        return;
    }

    // 入口处快照传输参数（值拷贝）：UpgradeWorker 线程内经 SendFunc 发送时
    // 不再读取任何 QWidget 成员，避免跨线程访问 UI 控件。
    const TransportType transport = m_transport;
    const quint16 defaultPort =
        static_cast<quint16>(m_connect ? m_connect->getDevicePort() : IapCommands::IAP_PORT);
    m_engine->setSendFunc([this, transport, defaultPort](const QByteArray& data, Device* device) {
        sendToDevice(data, device, transport, defaultPort);
    });
    // 广播函数（仅 UDP）：升级前置重启后 worker 用 4B01 广播搜索确认设备
    // 重新上线（Recovery 应答 rtn_cmd01 广播）。同样在入口处快照，避免
    // worker 线程触碰 UI 成员；串口模式留空（worker 走固定延时路径）。
    UpgradeEngine::BroadcastFunc broadcastFunc;
    if (transport == TransportType::Udp && m_udp) {
        broadcastFunc = [this, defaultPort](const QByteArray& data) {
            m_udp->sendBroadcast(data, defaultPort);
        };
    }
    m_engine->setBroadcastFunc(broadcastFunc);

    m_upgradePanel->setUpgrading(true);
    m_engine->startUpgrade(devices);
}

void IapPage::onCancelUpgrade()
{
    m_engine->cancelAll();
    m_upgradePanel->setUpgrading(false);
}

void IapPage::onReboot()
{
    const TransportType transport = m_transport;
    const quint16 defaultPort =
        static_cast<quint16>(m_connect ? m_connect->getDevicePort() : IapCommands::IAP_PORT);
    for (Device* d : m_deviceMgr->getSelectedDevices()) {
        sendToDevice(IapCommands::buildRebootRequest(), d, transport, defaultPort);
        appendLog(QStringLiteral("已发送重启命令到 %1").arg(d->displayName()),
                  QStringLiteral("CMD"));
    }
}

void IapPage::onRecovery()
{
    const TransportType transport = m_transport;
    const quint16 defaultPort =
        static_cast<quint16>(m_connect ? m_connect->getDevicePort() : IapCommands::IAP_PORT);
    for (Device* d : m_deviceMgr->getSelectedDevices()) {
        sendToDevice(IapCommands::buildEnterRecoveryRequest(), d, transport, defaultPort);
        appendLog(QStringLiteral("已发送恢复模式命令到 %1").arg(d->displayName()),
                  QStringLiteral("CMD"));
    }
}

Device* IapPage::currentDevice() const
{
    if (m_currentDeviceId.isEmpty())
        return nullptr;
    return m_deviceMgr->getDevice(m_currentDeviceId);
}

namespace {
// IPv4 合法性：QHostAddress 解析为 IPv4 且四段数字均有效（setAddress 对
// "1.2.3.4" 类字符串返回 true；拒绝 IPv6 / 域名形式）。
bool isValidIpv4(const QString& text)
{
    QHostAddress addr;
    return addr.setAddress(text.trimmed())
        && addr.protocol() == QAbstractSocket::IPv4Protocol;
}
} // namespace

void IapPage::onConfigApply(const QString& ip, const QString& mask, const QString& gateway,
                            const QString& portText)
{
    Device* d = currentDevice();
    if (!d) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先扫描并选择设备"));
        return;
    }
    if (!isValidIpv4(ip) || !isValidIpv4(mask) || !isValidIpv4(gateway)) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("IP / 掩码 / 网关格式非法（须为 IPv4 点分十进制）"));
        return;
    }
    bool ok = false;
    const uint port = portText.toUInt(&ok);
    if (!ok || port < 1 || port > 65535) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("端口须为 1~65535"));
        return;
    }

    const TransportType transport = m_transport;
    const quint16 defaultPort =
        static_cast<quint16>(m_connect ? m_connect->getDevicePort() : IapCommands::IAP_PORT);
    // 4B02 单播到设备 IP:10011（与 4B06/4B07 一致；主固件 netconn 绑 ANY，
    // 任意源可收）。应答 rtn_cmd02 经广播回，由 DeviceManager 两态解析后
    // 触发 setIpAck。
    sendToDevice(IapCommands::buildSetIpRequest(ip, mask, gateway,
                                                static_cast<quint16>(port)),
                 d, transport, defaultPort);
    appendLog(QStringLiteral("已发送配置下发命令到 %1 (%2/%3/%4/%5)")
                  .arg(d->displayName(), ip, mask, gateway)
                  .arg(port),
              QStringLiteral("CMD"));
}

void IapPage::onQueryStatus()
{
    Device* d = currentDevice();
    if (!d) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先扫描并选择设备"));
        return;
    }
    const TransportType transport = m_transport;
    const quint16 defaultPort =
        static_cast<quint16>(m_connect ? m_connect->getDevicePort() : IapCommands::IAP_PORT);
    sendToDevice(IapCommands::buildQueryStatusRequest(), d, transport, defaultPort);
    appendLog(QStringLiteral("已发送固件状态查询 (4B03) 到 %1").arg(d->displayName()),
              QStringLiteral("CMD"));
}

void IapPage::onSetIpAck(const QString& id, bool ok)
{
    Q_UNUSED(id);
    if (ok) {
        appendLog(QStringLiteral("设备确认配置下发"), QStringLiteral("SUCCESS"));
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("已下发，设备重启后生效，请重新搜索设备"));
    } else {
        appendLog(QStringLiteral("设备拒绝配置下发"), QStringLiteral("ERROR"));
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("配置下发失败（设备返回错误结果码）"));
    }
}

void IapPage::onFrameReceived(const QByteArray& data, const QString& srcIp, quint16 /*srcPort*/)
{
    IapFrame::ParsedFrame parsed;
    if (!IapFrame::parseFrame(data, &parsed)) {
        appendLog(QStringLiteral("[%1] IAP 帧解析失败").arg(srcIp), QStringLiteral("WARN"));
        return;
    }
    handleParsedFrame(parsed, srcIp, TransportType::Udp);
}

void IapPage::onBytesReceived(const QByteArray& data)
{
    m_rxBuffer.append(data);
    const QString srcId = m_connect ? m_connect->getSerialPort() : QString();

    while (m_rxBuffer.size() >= IapFrame::HEADER_SIZE_BYTES) {
        const quint32 magic = IapFrame::readLe32(m_rxBuffer, 0);
        if (magic != IapFrame::FRAME_MAGIC) {
            m_rxBuffer.remove(0, 1);
            continue;
        }
        const quint32 payloadLen = IapFrame::readLe32(m_rxBuffer, 12);
        if (payloadLen > (1u << 20)) { // sanity: reject absurd lengths
            m_rxBuffer.remove(0, 1);
            continue;
        }
        const int frameSize = IapFrame::HEADER_SIZE_BYTES + static_cast<int>(payloadLen) * 4 + 4;
        if (m_rxBuffer.size() < frameSize)
            break;

        const QByteArray frame = m_rxBuffer.left(frameSize);
        m_rxBuffer.remove(0, frameSize);

        IapFrame::ParsedFrame parsed;
        if (!IapFrame::parseFrame(frame, &parsed)) {
            appendLog(QStringLiteral("[%1] IAP 帧解析失败").arg(srcId), QStringLiteral("WARN"));
            continue;
        }
        handleParsedFrame(parsed, srcId, TransportType::Serial);
    }
}

void IapPage::handleParsedFrame(const IapFrame::ParsedFrame& parsed, const QString& srcId,
                                TransportType transport)
{
    if (!parsed.validCrc) {
        appendLog(QStringLiteral("[%1] CRC校验失败: 0x%2")
                      .arg(srcId)
                      .arg(QString::number(parsed.cmd, 16).toUpper().rightJustified(8, QLatin1Char('0'))),
                  QStringLiteral("ERROR"));
        return;
    }
    m_deviceMgr->handleFrame(parsed, srcId, transport);
}

void IapPage::onEngineProgress(const QString& id, double percent, int current, int total)
{
    Device* d = m_deviceMgr->getDevice(id);
    if (d) {
        d->progress = static_cast<int>(percent);
        m_devicePanel->updateDevice(d);
    }

    const QVector<Device*> online = m_deviceMgr->getSelectedDevices();
    if (!online.isEmpty()) {
        double sum = 0.0;
        for (Device* dd : online)
            sum += dd->progress;
        m_upgradePanel->updateProgress(sum / online.size(), current, total);
    }
}

void IapPage::onEngineStatus(const QString& id, DeviceStatus status)
{
    Device* d = m_deviceMgr->getDevice(id);
    if (d) {
        d->status.store(status);
        m_devicePanel->updateDevice(d);
    }
}

void IapPage::onEngineState(EngineState state)
{
    m_upgradePanel->updateStatus(engineStateLabel(state));
    if (state == EngineState::Done || state == EngineState::Failed) {
        m_upgradePanel->setUpgrading(false);
        if (state == EngineState::Done)
            appendLog(QStringLiteral("所有设备升级成功!"), QStringLiteral("SUCCESS"));
        else
            appendLog(QStringLiteral("部分设备升级失败"), QStringLiteral("ERROR"));
    }
}

void IapPage::onTransportError(const QString& msg)
{
    appendLog(msg, QStringLiteral("ERROR"));
}

void IapPage::sendToDevice(const QByteArray& data, Device* device, TransportType transport,
                           quint16 defaultPort)
{
    if (transport == TransportType::Udp) {
        if (m_udp) {
            // 单播目标恒为 IAP 口（defaultPort = 连接面板「设备口」，默认
            // IapCommands::IAP_PORT=10011，与搜索广播同一端口源）。设备 4B01
            // 应答第 4 word 上报的是 TCP 业务口（9528，存于 Device::appPort），
            // 仅作 UI 显示/第三方工具连接参考——设备 IAP 协议只监听 10011，
            // 若把 appPort 用作单播目标，升级/重启/Recovery 帧会发错端口。
            m_udp->sendUnicast(data, device->ip, defaultPort);
        }
    } else {
        if (m_serial)
            m_serial->send(data);
    }
}

void IapPage::appendLog(const QString& msg, const QString& level)
{
    if (m_log)
        m_log->append(msg, level);
}
