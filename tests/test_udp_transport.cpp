#include <QtTest>

#include <QCoreApplication>
#include <memory>

#include "protocol/iap/IapCommands.h"
#include "transport/UdpTransport.h"

// UDP 广播网络层回归测试（2026-08-25 修复）：
// 根因是 QUdpSocket 底层 socket 未开 SO_BROADCAST——Linux 下向 255.255.255.255
// / 定向广播地址 sendto 返回 EACCES，搜索帧从未上链路（Windows 端 Java 参考
// 工具 DatagramSocket 默认开启 SO_BROADCAST，故正常）。
class TestUdpTransport : public QObject {
    Q_OBJECT

private slots:
    void testBroadcastOptionEnabled();
    void testLoopbackBroadcastRoundTrip();
};

void TestUdpTransport::testBroadcastOptionEnabled()
{
    UdpTransport t;
    t.start();
    t.bind(20011, QString()); // 通配绑定 0.0.0.0:20011

    // bind 异步入队到传输线程，轮询直到底层 fd 就绪且 SO_BROADCAST 已开。
    QTRY_VERIFY_WITH_TIMEOUT(t.isBroadcastEnabled(), 3000);
    t.stop();
}

void TestUdpTransport::testLoopbackBroadcastRoundTrip()
{
    UdpTransport t;
    t.start();
    t.bind(20012, QString());

    QByteArray received;
    bool gotFrame = false;
    QString errorMsg;
    QObject::connect(&t, &UdpTransport::frameReceived,
                     [&](const QByteArray& data, const QString&, quint16) {
                         received = data;
                         gotFrame = true;
                     });
    QObject::connect(&t, &UdpTransport::errorOccurred,
                     [&](const QString& msg) { errorMsg = msg; });

    QTRY_VERIFY_WITH_TIMEOUT(t.isBroadcastEnabled(), 3000);

    const QByteArray frame = IapCommands::buildReportIpRequest();
    t.sendBroadcast(frame, 20012);

#ifdef Q_OS_WIN
    // Windows 对本地发起的 255.255.255.255 不保证回环投递给本机 socket
    // （需 IP_RECEIVE_BROADCAST），故只验证无发送错误。
    QTest::qWait(1000);
    QVERIFY2(errorMsg.isEmpty(), qPrintable(errorMsg));
#else
    // Linux 将本地广播回环投递给本机通配绑定的 socket：收包即证明
    // SO_BROADCAST 生效且帧原样上链路。
    QTRY_VERIFY_WITH_TIMEOUT(gotFrame, 3000);
    QCOMPARE(received, frame);
#endif
    t.stop();
}

int runTestUdpTransport(int argc, char** argv)
{
    // 本测试涉及真实 QThread + BlockingQueuedConnection 跨线程查询，必须有
    // 事件循环宿主；测试入口 main() 未创建 QCoreApplication（其余测试无事件
    // 依赖），此处局部创建（已存在则不重复创建）。
    std::unique_ptr<QCoreApplication> app;
    if (!QCoreApplication::instance())
        app = std::make_unique<QCoreApplication>(argc, argv);

    TestUdpTransport tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "test_udp_transport.moc"