#include "UdpTransport.h"

#include <QAbstractSocket>
#include <QHostAddress>
#include <QList>
#include <QMetaObject>
#include <QNetworkInterface>
#include <QUdpSocket>

UdpTransport::UdpTransport(QObject* parent)
    : QObject(parent)
{
    moveToThread(&m_thread);
    connect(&m_thread, &QThread::started, this, &UdpTransport::onThreadStarted);
}

UdpTransport::~UdpTransport()
{
    stop();
}

void UdpTransport::start()
{
    if (!m_thread.isRunning())
        m_thread.start();
}

void UdpTransport::stop()
{
    if (m_thread.isRunning()) {
        QMetaObject::invokeMethod(this, "doShutdown", Qt::QueuedConnection);
        m_thread.quit();
        m_thread.wait();
    }
}

void UdpTransport::bind(quint16 port, const QString& bindIp)
{
    m_port = port;
    m_bindIp = bindIp;
    m_bindRequested = true;
    if (m_thread.isRunning())
        QMetaObject::invokeMethod(this, "doBind", Qt::QueuedConnection,
                                  Q_ARG(quint16, port), Q_ARG(QString, bindIp));
}

void UdpTransport::sendBroadcast(const QByteArray& data, quint16 port)
{
    if (!m_thread.isRunning())
        return;
    QMetaObject::invokeMethod(this, "doSendBroadcast", Qt::QueuedConnection,
                              Q_ARG(QByteArray, data), Q_ARG(quint16, port));
}

void UdpTransport::sendUnicast(const QByteArray& data, const QString& ip, quint16 port)
{
    if (!m_thread.isRunning())
        return;
    QMetaObject::invokeMethod(this, "doSendUnicast", Qt::QueuedConnection,
                              Q_ARG(QByteArray, data), Q_ARG(QString, ip), Q_ARG(quint16, port));
}

void UdpTransport::onThreadStarted()
{
    if (!m_socket) {
        m_socket = new QUdpSocket(this);
        connect(m_socket, &QUdpSocket::readyRead, this, [this]() {
            while (m_socket->hasPendingDatagrams()) {
                const qint64 size = m_socket->pendingDatagramSize();
                if (size <= 0)
                    break;
                QByteArray data(static_cast<int>(size), Qt::Uninitialized);
                QHostAddress src;
                quint16 srcPort = 0;
                const qint64 n = m_socket->readDatagram(data.data(), size, &src, &srcPort);
                if (n < 0)
                    break;
                data.resize(static_cast<int>(n));
                emit frameReceived(data, src.toString(), srcPort);
            }
        });
    }
    if (m_bindRequested)
        doBind(m_port, m_bindIp);
}

void UdpTransport::doBind(quint16 port, const QString& bindIp)
{
    m_port = port;
    m_bindIp = bindIp;

    if (!m_socket) {
        emit errorOccurred(QStringLiteral("UDP socket 未初始化"));
        return;
    }
    if (m_socket->state() != QAbstractSocket::UnconnectedState)
        m_socket->close();

    const QHostAddress bindAddr = bindIp.isEmpty()
        ? QHostAddress(QHostAddress::AnyIPv4)
        : QHostAddress(bindIp);

    if (!m_socket->bind(bindAddr, port, QUdpSocket::ShareAddress)) {
        emit errorOccurred(QStringLiteral("绑定 UDP 端口 %1 失败: %2")
                               .arg(QString::number(port), m_socket->errorString()));
        return;
    }
    m_socket->joinMulticastGroup(QHostAddress(QStringLiteral("224.0.0.1")));
}

void UdpTransport::doShutdown()
{
    if (m_socket)
        m_socket->close();
}

void UdpTransport::doSendBroadcast(const QByteArray& data, quint16 port)
{
    if (!m_socket || m_socket->state() != QAbstractSocket::BoundState)
        return;

    if (!m_bindIp.isEmpty()) {
        const QList<QNetworkInterface> ifaces = QNetworkInterface::allInterfaces();
        for (const QNetworkInterface& iface : ifaces) {
            for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
                if (entry.ip().toString() != m_bindIp)
                    continue;
                const QHostAddress bc = entry.broadcast();
                if (!bc.isNull() && bc.toString() != QStringLiteral("0.0.0.0")) {
                    m_socket->writeDatagram(data, bc, port);
                    return;
                }
            }
        }
    }
    m_socket->writeDatagram(data, QHostAddress(QHostAddress::Broadcast), port);
}

void UdpTransport::doSendUnicast(const QByteArray& data, const QString& ip, quint16 port)
{
    if (!m_socket || m_socket->state() != QAbstractSocket::BoundState)
        return;
    m_socket->writeDatagram(data, QHostAddress(ip), port);
}
