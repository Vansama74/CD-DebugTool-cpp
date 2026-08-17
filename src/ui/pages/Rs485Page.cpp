#include "Rs485Page.h"

#include "Rs485ControlPanel.h"

#include "protocol/rs485/Rs485Commands.h"
#include "protocol/rs485/Rs485Frame.h"
#include "transport/SerialTransport.h"
#include "ui/widgets/ConnectConfigPanel.h"
#include "ui/widgets/HexDumpPanel.h"
#include "ui/widgets/LogPanel.h"

#include <QVBoxLayout>

Rs485Page::Rs485Page(QWidget* parent)
    : IProtocolPage(parent)
{
    m_controlPanel = new Rs485ControlPanel(this);
    m_transport = new SerialTransport(); // deliberately unparented

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_controlPanel);

    // Connection config -> transport.
    connect(m_controlPanel->connectPanel(), &ConnectConfigPanel::portOpened,
            this, &Rs485Page::onPortOpened);
    connect(m_controlPanel->connectPanel(), &ConnectConfigPanel::portClosed,
            this, &Rs485Page::onPortClosed);

    // Control panel -> transport / log.
    connect(m_controlPanel, &Rs485ControlPanel::sendFrame,
            m_transport, &SerialTransport::send);
    connect(m_controlPanel, &Rs485ControlPanel::logMessage,
            this, [this](const QString& msg, const QString& level) {
                if (m_log)
                    m_log->append(msg, level);
            });

    // Transport -> page.
    connect(m_transport, &SerialTransport::bytesReceived,
            this, &Rs485Page::onBytesReceived);
    connect(m_transport, &SerialTransport::connected,
            this, &Rs485Page::onTransportConnected);
    connect(m_transport, &SerialTransport::disconnected,
            this, &Rs485Page::onTransportDisconnected);
    // 串口意外断开（拔线等）时回写连接面板状态，避免“假连接”。
    connect(m_transport, &SerialTransport::disconnected,
            m_controlPanel->connectPanel(), &ConnectConfigPanel::onTransportDisconnected);
    connect(m_transport, &SerialTransport::errorOccurred,
            this, &Rs485Page::onTransportError);
}

Rs485Page::~Rs485Page()
{
    delete m_transport;
    m_transport = nullptr;
}

QString Rs485Page::key() const { return QStringLiteral("rs485"); }
QString Rs485Page::fullName() const { return QStringLiteral("重庆创迪车道指示器"); }

void Rs485Page::activate()
{
    if (m_transport)
        m_transport->start();
}

void Rs485Page::deactivate()
{
    // Reset the connect panel first so its portClosed signal can close cleanly.
    if (m_controlPanel && m_controlPanel->connectPanel()
        && m_controlPanel->connectPanel()->isSerialOpen()) {
        m_controlPanel->connectPanel()->setSerialOpenState(false);
    }
    if (m_transport)
        m_transport->stop();
}

void Rs485Page::onPortOpened(const QString& port, int baud)
{
    if (m_transport)
        m_transport->open(port, baud);
}

void Rs485Page::onPortClosed()
{
    if (m_transport)
        m_transport->close();
}

void Rs485Page::onTransportConnected(const QString& port, int baud)
{
    if (m_log)
        m_log->append(QStringLiteral("RS485 串口 %1 @ %2 已打开").arg(port).arg(baud),
                      QStringLiteral("SUCCESS"));
}

void Rs485Page::onTransportDisconnected()
{
    if (m_log)
        m_log->append(QStringLiteral("RS485 串口已关闭"), QStringLiteral("INFO"));
}

void Rs485Page::onTransportError(const QString& msg)
{
    if (m_log)
        m_log->append(msg, QStringLiteral("ERROR"));
}

void Rs485Page::onBytesReceived(const QByteArray& data)
{
    m_rxBuffer.append(data);

    // Extract complete 6-byte frames (header 0xCC ... tail 0xDD).
    while (m_rxBuffer.size() >= Rs485Frame::FRAME_SIZE) {
        if (static_cast<quint8>(m_rxBuffer.at(0)) != Rs485Frame::HEADER) {
            m_rxBuffer.remove(0, 1);
            continue;
        }
        if (static_cast<quint8>(m_rxBuffer.at(5)) != Rs485Frame::TAIL) {
            m_rxBuffer.remove(0, 1);
            continue;
        }
        const QByteArray frame = m_rxBuffer.left(Rs485Frame::FRAME_SIZE);
        m_rxBuffer.remove(0, Rs485Frame::FRAME_SIZE);
        onFrameReceived(frame);
    }
}

void Rs485Page::onFrameReceived(const QByteArray& frame)
{
    quint8 deviceId = 0, cmd = 0, dataByte = 0;
    bool valid = false;
    if (!Rs485Frame::parseFrame(frame, &deviceId, &cmd, &dataByte, &valid)) {
        if (m_log)
            m_log->append(QStringLiteral("<<< READ: %1 解析失败")
                              .arg(Rs485Frame::frameToHex(frame)),
                          QStringLiteral("WARN"));
        return;
    }

    const QString hex = Rs485Frame::frameToHex(frame);
    m_controlPanel->hexPanel()->appendRx(hex);

    if (!valid) {
        if (m_log)
            m_log->append(QStringLiteral("<<< READ: %1 (校验错)").arg(hex),
                          QStringLiteral("ERROR"));
        return;
    }

    const QString desc = Rs485Commands::describeResponse(cmd, dataByte);
    if (!desc.isEmpty()) {
        if (m_log)
            m_log->append(QStringLiteral("<<< READ: %1 | %2 | %3")
                              .arg(hex, Rs485Commands::cmdName(cmd), desc),
                          QStringLiteral("SUCCESS"));
    } else {
        const QString tag =
            (cmd < 0x80) ? QStringLiteral("ECHO") : QStringLiteral("应答");
        if (m_log)
            m_log->append(QStringLiteral("<<< READ: %1 (%2: %3)")
                              .arg(hex, tag, Rs485Commands::cmdName(cmd)),
                          QStringLiteral("SUCCESS"));
    }

    m_controlPanel->handleResponse(frame);
}
