// 平台头文件必须先于 Qt 头（Windows 下 winsock2.h 必须先于 windows.h，
// 否则 windows.h 引入 winsock.h 导致类型重定义）。
// Q_OS_WIN 由 qglobal.h 提供，必须先包含 QtGlobal 再判断，否则
// 顶层条件编译时该宏尚未定义，Windows 下会误入 POSIX 分支。
#include <QtGlobal>

#ifdef Q_OS_WIN
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif

#include "UdpTransport.h"

#include <QAbstractSocket>
#include <QHostAddress>
#include <QList>
#include <QMetaObject>
#include <QNetworkInterface>
#include <QSet>
#include <QUdpSocket>

namespace {

// Qt 5.15 无 SO_BROADCAST 的可移植 API（QAbstractSocket::BroadcastSocketOption
// 到 Qt 6.2 才引入）。QUdpSocket 内部 socket 默认**不设置** SO_BROADCAST，
// Linux 下向 255.255.255.255 / 定向广播地址 sendto 直接返回 EACCES
//（Permission denied），搜索帧被静默丢弃——这就是 Linux 端搜不到设备的根因。
// 对照：Java 参考工具 DatagramSocket 构造即默认开启 SO_BROADCAST
//（JDK getBroadcast()==true），Windows 端无需任何设置即可广播。
void enableBroadcastOption(const QUdpSocket& socket)
{
    const qintptr fd = socket.socketDescriptor();
    if (fd == -1)
        return;
    const int on = 1;
#ifdef Q_OS_WIN
    ::setsockopt(static_cast<SOCKET>(fd), SOL_SOCKET, SO_BROADCAST,
                 reinterpret_cast<const char*>(&on), sizeof(on));
#else
    ::setsockopt(static_cast<int>(fd), SOL_SOCKET, SO_BROADCAST, &on, sizeof(on));
#endif
}

bool broadcastOptionEnabled(const QUdpSocket& socket)
{
    const qintptr fd = socket.socketDescriptor();
    if (fd == -1)
        return false;
    int val = 0;
#ifdef Q_OS_WIN
    int len = sizeof(val);
    if (::getsockopt(static_cast<SOCKET>(fd), SOL_SOCKET, SO_BROADCAST,
                     reinterpret_cast<char*>(&val), &len) != 0)
        return false;
#else
    socklen_t len = sizeof(val);
    if (::getsockopt(static_cast<int>(fd), SOL_SOCKET, SO_BROADCAST, &val, &len) != 0)
        return false;
#endif
    return val != 0;
}

} // namespace

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

    // 必须通配绑定 0.0.0.0，不能绑定所选网卡的具体 IP：设备 4B01 应答以
    // **广播**（dst 255.255.255.255）发出（tcpdump 实证：
    // 192.168.114.200.10011 > 255.255.255.255.10011，B 标志），Linux 下
    // 绑定具体单播 IP 的 UDP socket 收不到目的地址为全局广播的入站报文，
    // 只会把应答投递给通配绑定（或该广播对应接口地址）的 socket。bindIp
    // 参数保留但不再用于绑定地址（见 sendBroadcast 的全接口广播策略）。
    const QHostAddress bindAddr(QHostAddress::AnyIPv4);

    if (!m_socket->bind(bindAddr, port, QUdpSocket::ShareAddress)) {
        emit errorOccurred(QStringLiteral("绑定 UDP 端口 %1 失败: %2")
                               .arg(QString::number(port), m_socket->errorString()));
        return;
    }
    // 绑定后开启 SO_BROADCAST（bind 重新创建底层 fd，选项必须在此之后设置）。
    enableBroadcastOption(*m_socket);
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

    // 搜索语义：广播到本机所有 IPv4 广播域。旧实现仅向所选网卡的定向广播发送
    // ——多网段主机（本开发机 enp193s0 单网卡同时挂 192.168.0/1/2/114.x 四个
    // 网段，面板设备在 114 网段）上，若组合框默认选中 192.168.0.17，定向广播
    // 只发 192.168.0.255，114 网段的设备永远收不到。
    //
    // 发送顺序约束（2026-08-25 修复「4B04 擦除超时」，必须保持：定向广播在前、
    // 全局广播在后）：
    // Recovery 固件 `udp_receive_callback` 收到**第一帧**即 `udp_connect(远端
    // ip:port)` 锁定 pcb（STM32F407-Recovery/LWIP/App/udp_conn.c），此后 LwIP
    // 只接受「源 ip:port 与该锁定远端一致」的数据报，其余单播直接回 ICMP
    // udp port 10011 unreachable（在线 tcpdump 实证）。
    //   1) 每个 up 接口的定向广播：经所在子网发出，源 IP = 本机该子网地址，
    //      与后续发往该子网设备的**单播源 IP 一致**（同一条内核路由）。设备
    //      先收到定向广播 → 锁定到正确的本机子网地址 → 后续单播可被接受。
    //   2) 255.255.255.255 全局广播**最后**发（与 Java 参考工具一致，经默认
    //      路由网卡发出，源 IP = 默认路由源地址）。本机多网段时该源地址 ≠
    //      设备子网的源地址——若全局广播先行，设备会锁定到默认路由源地址
    //      （如 192.168.0.17），而单播走设备子网路由（源 192.168.114.17），
    //      被设备拒收 → 升级擦除 30s 无 ACK。全局广播兜底：设备不在本机任何
    //      子网时单播也走默认路由（源地址仍与全局广播一致），语义不破坏。
    // 设备应答回源到绑定 socket（src 为发帧所用的本地地址），原路收取。
    int sent = 0;
    const auto sendTo = [this, &sent, &data, port](const QHostAddress& addr) {
        if (m_socket->writeDatagram(data, addr, port) == data.size())
            ++sent;
    };

    QSet<quint32> seen;
    const QList<QNetworkInterface> ifaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface& iface : ifaces) {
        if (!(iface.flags() & QNetworkInterface::IsUp))
            continue;
        if (iface.flags() & QNetworkInterface::IsLoopBack)
            continue;
        for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
            if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol)
                continue;
            const QHostAddress bc = entry.broadcast();
            if (bc.isNull() || bc == QHostAddress(QHostAddress::AnyIPv4) ||
                bc == QHostAddress(QHostAddress::Broadcast))
                continue;
            if (seen.contains(bc.toIPv4Address()))
                continue;
            seen.insert(bc.toIPv4Address());
            sendTo(bc);
        }
    }

    // 全局广播兜底（必须最后发送，见上方发送顺序约束注释）。
    sendTo(QHostAddress(QHostAddress::Broadcast));

    if (sent == 0)
        emit errorOccurred(QStringLiteral("UDP 广播发送失败: %1").arg(m_socket->errorString()));
}

void UdpTransport::doSendUnicast(const QByteArray& data, const QString& ip, quint16 port)
{
    if (!m_socket || m_socket->state() != QAbstractSocket::BoundState)
        return;
    m_socket->writeDatagram(data, QHostAddress(ip), port);
}

bool UdpTransport::isBroadcastEnabled()
{
    if (QThread::currentThread() == &m_thread)
        return m_socket && broadcastOptionEnabled(*m_socket);
    if (!m_thread.isRunning())
        return false;
    bool result = false;
    QMetaObject::invokeMethod(
        this,
        [this, &result]() { result = m_socket && broadcastOptionEnabled(*m_socket); },
        Qt::BlockingQueuedConnection);
    return result;
}
