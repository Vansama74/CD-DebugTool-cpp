#include "ConnectConfigPanel.h"

#include <QAbstractSocket>
#include <QComboBox>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QNetworkInterface>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStyle>
#include <QVBoxLayout>

#include "config/ConfigManager.h"
#include "transport/SerialTransport.h"

ConnectConfigPanel::ConnectConfigPanel(ProtocolConnectMode mode, QWidget* parent, int defaultBaud)
    : QWidget(parent)
    , m_mode(mode)
    , m_defaultBaud(defaultBaud)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(6, 2, 6, 2);
    root->setSpacing(4);

    auto* group = new QGroupBox(QStringLiteral("连接配置"), this);
    auto* groupLayout = new QVBoxLayout(group);
    groupLayout->setContentsMargins(6, 6, 6, 6);
    groupLayout->setSpacing(4);

    if (mode == ProtocolConnectMode::UdpAndSerial) {
        auto* row = new QHBoxLayout();
        row->addWidget(new QLabel(QStringLiteral("传输方式:"), group));
        m_transportCombo = new QComboBox(group);
        m_transportCombo->addItems({QStringLiteral("UDP 网络"), QStringLiteral("串口")});
        connect(m_transportCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &ConnectConfigPanel::onTransportChanged);
        row->addWidget(m_transportCombo);
        row->addStretch(1);
        groupLayout->addLayout(row);

        m_transportStack = new QStackedWidget(group);
        m_transportStack->addWidget(buildUdpConfig());    // page 0
        m_transportStack->addWidget(buildSerialConfig()); // page 1
        groupLayout->addWidget(m_transportStack);
    } else if (mode == ProtocolConnectMode::UdpOnly) {
        groupLayout->addWidget(buildUdpConfig());
    } else { // SerialOnly
        groupLayout->addWidget(buildSerialConfig());
    }

    root->addWidget(group);

    if (mode != ProtocolConnectMode::UdpOnly) {
        refreshSerialPorts();
        applySavedSerialConfig();
    }
    if (mode != ProtocolConnectMode::SerialOnly)
        refreshNetworkInterfaces();
}

QWidget* ConnectConfigPanel::buildUdpConfig()
{
    auto* w = new QWidget();
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(0, 4, 0, 0);
    lay->setSpacing(6);

    auto* nicRow = new QHBoxLayout();
    nicRow->addWidget(new QLabel(QStringLiteral("本机网卡:"), w));
    m_nicCombo = new QComboBox(w);
    m_nicCombo->setMinimumWidth(200);
    connect(m_nicCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { onNicChanged(); });
    nicRow->addWidget(m_nicCombo, 1);
    lay->addLayout(nicRow);

    auto* ipRow = new QHBoxLayout();
    ipRow->addWidget(new QLabel(QStringLiteral("本机IP:"), w));
    m_localIpLabel = new QLabel(QStringLiteral("--"), w);
    m_localIpLabel->setStyleSheet(QStringLiteral("color: #89B4FA; font-weight: bold;"));
    ipRow->addWidget(m_localIpLabel);
    ipRow->addStretch(1);
    lay->addLayout(ipRow);

    const QJsonObject cfg = ConfigManager::load();

    auto* listenRow = new QHBoxLayout();
    listenRow->addWidget(new QLabel(QStringLiteral("监听端口:"), w));
    m_listenSpin = new QSpinBox(w);
    m_listenSpin->setRange(1, 65535);
    m_listenSpin->setValue(ConfigManager::getInt(cfg, "listen_port", 10011));
    listenRow->addWidget(m_listenSpin);
    listenRow->addStretch(1);
    lay->addLayout(listenRow);

    auto* deviceRow = new QHBoxLayout();
    deviceRow->addWidget(new QLabel(QStringLiteral("设备端口:"), w));
    m_deviceSpin = new QSpinBox(w);
    m_deviceSpin->setRange(1, 65535);
    m_deviceSpin->setValue(ConfigManager::getInt(cfg, "device_port", 10011));
    deviceRow->addWidget(m_deviceSpin);
    deviceRow->addStretch(1);
    lay->addLayout(deviceRow);

    return w;
}

QWidget* ConnectConfigPanel::buildSerialConfig()
{
    auto* w = new QWidget();
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(0, 4, 0, 0);
    lay->setSpacing(6);

    auto* row = new QHBoxLayout();
    row->addWidget(new QLabel(QStringLiteral("串口:"), w));

    m_serialCombo = new QComboBox(w);
    m_serialCombo->setMinimumWidth(150);
    row->addWidget(m_serialCombo);

    row->addWidget(new QLabel(QStringLiteral("波特率:"), w));
    m_baudCombo = new QComboBox(w);
    m_baudCombo->addItems({QStringLiteral("9600"), QStringLiteral("19200"),
                           QStringLiteral("38400"), QStringLiteral("57600"),
                           QStringLiteral("115200"), QStringLiteral("230400"),
                           QStringLiteral("460800"), QStringLiteral("921600")});
    const int baudIdx = m_baudCombo->findText(QString::number(m_defaultBaud));
    if (baudIdx >= 0)
        m_baudCombo->setCurrentIndex(baudIdx);
    else {
        m_baudCombo->addItem(QString::number(m_defaultBaud));
        m_baudCombo->setCurrentIndex(m_baudCombo->count() - 1);
    }
    row->addWidget(m_baudCombo);

    m_refreshBtn = new QPushButton(QStringLiteral("刷新"), w);
    m_refreshBtn->setCursor(Qt::PointingHandCursor);
    connect(m_refreshBtn, &QPushButton::clicked, this, &ConnectConfigPanel::refreshSerialPorts);
    row->addWidget(m_refreshBtn);

    auto* sep = new QFrame(w);
    sep->setFrameShape(QFrame::VLine);
    sep->setFrameShadow(QFrame::Sunken);
    row->addWidget(sep);

    m_openBtn = new QPushButton(QStringLiteral("打开串口"), w);
    m_openBtn->setObjectName(QStringLiteral("primaryBtn"));
    m_openBtn->setMinimumWidth(100);
    m_openBtn->setCursor(Qt::PointingHandCursor);
    connect(m_openBtn, &QPushButton::clicked, this, &ConnectConfigPanel::togglePort);
    row->addWidget(m_openBtn);

    m_statusLabel = new QLabel(QStringLiteral("● 未连接"), w);
    m_statusLabel->setStyleSheet(QStringLiteral("color: #F38BA8; font-weight: bold;"));
    m_statusLabel->setMinimumWidth(80);
    row->addWidget(m_statusLabel);

    row->addStretch(1);
    lay->addLayout(row);

    return w;
}

void ConnectConfigPanel::refreshSerialPorts()
{
    if (!m_serialCombo)
        return;

    const QString current = m_serialCombo->currentData().toString();
    m_serialCombo->clear();

    const QStringList ports = SerialTransport::availablePorts();
    for (const QString& loc : ports)
        m_serialCombo->addItem(SerialTransport::portLabel(loc), loc);
    if (ports.isEmpty())
        m_serialCombo->addItem(QStringLiteral("未检测到串口设备"), QString());

    const int idx = m_serialCombo->findData(current);
    if (idx >= 0)
        m_serialCombo->setCurrentIndex(idx);
}

void ConnectConfigPanel::refreshNetworkInterfaces()
{
    if (!m_nicCombo)
        return;

    const QString current = m_nicCombo->currentData().toString();
    m_nicCombo->clear();

    const QList<QNetworkInterface> ifaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface& iface : ifaces) {
        const QNetworkInterface::InterfaceFlags flags = iface.flags();
        if (!(flags & QNetworkInterface::IsUp))
            continue;
        if (flags & QNetworkInterface::IsLoopBack)
            continue;
        if (flags & QNetworkInterface::IsPointToPoint)
            continue;

        for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
            if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol)
                continue;
            const QString ipv4 = entry.ip().toString();
            if (ipv4.isEmpty() || ipv4.startsWith(QStringLiteral("127.")))
                continue;
            m_nicCombo->addItem(iface.humanReadableName() + QStringLiteral("  [") + ipv4
                                    + QStringLiteral("]"),
                                ipv4);
        }
    }

    const int idx = m_nicCombo->findData(current);
    if (idx >= 0)
        m_nicCombo->setCurrentIndex(idx);
    onNicChanged();
}

void ConnectConfigPanel::onTransportChanged(int idx)
{
    if (m_transportStack)
        m_transportStack->setCurrentIndex(idx);
    if (idx == 1)
        refreshSerialPorts();
    emit transportChanged(idx);
}

void ConnectConfigPanel::onNicChanged()
{
    if (!m_nicCombo || !m_localIpLabel)
        return;
    const QString ip = m_nicCombo->currentData().toString();
    m_localIpLabel->setText(ip.isEmpty() ? QStringLiteral("--") : ip);
}

void ConnectConfigPanel::togglePort()
{
    applyOpenState(!m_serialOpen);
}

void ConnectConfigPanel::onTransportDisconnected()
{
    applyOpenState(false);
}

void ConnectConfigPanel::applySavedSerialConfig()
{
    const QJsonObject cfg = ConfigManager::load();
    const QString port = ConfigManager::getString(cfg, "last_serial_port", QString());
    const int baud = ConfigManager::getInt(cfg, "baud_rate", m_defaultBaud);

    if (m_serialCombo && !port.isEmpty()) {
        const int idx = m_serialCombo->findData(port);
        if (idx >= 0)
            m_serialCombo->setCurrentIndex(idx);
    }
    if (m_baudCombo) {
        const int idx = m_baudCombo->findText(QString::number(baud));
        if (idx >= 0)
            m_baudCombo->setCurrentIndex(idx);
    }
}

void ConnectConfigPanel::applyOpenState(bool open)
{
    if (open == m_serialOpen)
        return;
    m_serialOpen = open;

    if (open) {
        m_openBtn->setText(QStringLiteral("关闭串口"));
        m_openBtn->setObjectName(QStringLiteral("dangerBtn"));
        m_statusLabel->setText(QStringLiteral("● 已连接 %1").arg(getSerialPort()));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #A6E3A1; font-weight: bold;"));
        m_serialCombo->setEnabled(false);
        m_baudCombo->setEnabled(false);
        m_refreshBtn->setEnabled(false);

        // 打开串口时持久化端口与波特率，下次启动预选。
        QJsonObject cfg = ConfigManager::load();
        cfg.insert(QStringLiteral("last_serial_port"), getSerialPort());
        cfg.insert(QStringLiteral("baud_rate"), getBaudRate());
        ConfigManager::save(cfg);

        emit portOpened(getSerialPort(), getBaudRate());
    } else {
        m_openBtn->setText(QStringLiteral("打开串口"));
        m_openBtn->setObjectName(QStringLiteral("primaryBtn"));
        m_statusLabel->setText(QStringLiteral("● 未连接"));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #F38BA8; font-weight: bold;"));
        m_serialCombo->setEnabled(true);
        m_baudCombo->setEnabled(true);
        m_refreshBtn->setEnabled(true);
        emit portClosed();
    }

    // Re-polish so the objectName-driven QSS picks up the new role.
    m_openBtn->style()->unpolish(m_openBtn);
    m_openBtn->style()->polish(m_openBtn);
}

QString ConnectConfigPanel::getSerialPort() const
{
    return m_serialCombo ? m_serialCombo->currentData().toString() : QString();
}

int ConnectConfigPanel::getBaudRate() const
{
    return m_baudCombo ? m_baudCombo->currentText().toInt() : 115200;
}

QString ConnectConfigPanel::getSelectedNicIp() const
{
    return m_nicCombo ? m_nicCombo->currentData().toString() : QString();
}

int ConnectConfigPanel::getListenPort() const
{
    return m_listenSpin ? m_listenSpin->value() : 10011;
}

int ConnectConfigPanel::getDevicePort() const
{
    return m_deviceSpin ? m_deviceSpin->value() : 10011;
}

bool ConnectConfigPanel::isSerialOpen() const
{
    return m_serialOpen;
}

void ConnectConfigPanel::setSerialOpenState(bool open)
{
    applyOpenState(open);
}
