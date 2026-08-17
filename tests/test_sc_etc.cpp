#include <QtTest>

#include "protocol/sichuan_etc/SiChuanEtcProtocol.h"

namespace {

QByteArray bytes(std::initializer_list<int> vals)
{
    QByteArray b;
    b.reserve(static_cast<int>(vals.size()));
    for (int v : vals)
        b.append(static_cast<char>(v));
    return b;
}

} // namespace

class TestScEtc : public QObject {
    Q_OBJECT

private slots:
    void testDisplayFrame();
    void testDisplayTruncate();
    void testScrollFrame();
    void testClearAndInit();
    void testLightFrame();
    void testBrightnessFrame();
    void testHeartbeatFrame();
    void testAckScanner();
    void testAckScannerResync();
};

void TestScEtc::testDisplayFrame()
{
    // 全屏静态显示：0A 00 00 <GBK> 0D
    QCOMPARE(sc_etc::displayFrame(0, QStringLiteral("ETC")),
             bytes({0x0A, 0x00, 0x00, 'E', 'T', 'C', 0x0D}));
    // 第 2 行静态显示
    QCOMPARE(sc_etc::displayFrame(2, QStringLiteral("AB")),
             bytes({0x0A, 0x00, 0x02, 'A', 'B', 0x0D}));
}

void TestScEtc::testDisplayTruncate()
{
    // 全屏上限 56B：56 字节 ASCII 全部保留。
    const QString s56 = QString(56, QLatin1Char('A'));
    QCOMPARE(sc_etc::displayFrame(0, s56).size(), 60); // 0A 00 00 + 56 + 0D

    // 超过 56B 截断为 56B。
    const QString s60 = QString(60, QLatin1Char('A'));
    QCOMPARE(sc_etc::displayFrame(0, s60).size(), 60);

    // 单行上限 24B：12 个汉字 = 24B 保留；13 个汉字截断到 12 个（不切开汉字）。
    const QString hanzi12 = QStringLiteral("一二三四五六七八九十一二"); // 12 字
    QCOMPARE(sc_etc::displayFrame(1, hanzi12).size(), 28); // 0A 00 01 + 24 + 0D
    const QString hanzi13 = hanzi12 + QStringLiteral("三"); // 13 字
    QCOMPARE(sc_etc::displayFrame(1, hanzi13).size(), 28);
}

void TestScEtc::testScrollFrame()
{
    // 滚屏：0A 01 00 md rt st <数据> 0D
    QCOMPARE(sc_etc::scrollFrame(0x03, 0x02, 0xFF, QStringLiteral("AB")),
             bytes({0x0A, 0x01, 0x00, 0x03, 0x02, 0xFF, 'A', 'B', 0x0D}));
}

void TestScEtc::testClearAndInit()
{
    // 清全屏：0A 00 00 20 0D；清第 3 行：0A 00 03 20 0D
    QCOMPARE(sc_etc::clearFrame(0), bytes({0x0A, 0x00, 0x00, 0x20, 0x0D}));
    QCOMPARE(sc_etc::clearFrame(3), bytes({0x0A, 0x00, 0x03, 0x20, 0x0D}));
    // 初始化：0A 00 00 30 0D
    QCOMPARE(sc_etc::initFrame(), bytes({0x0A, 0x00, 0x00, 0x30, 0x0D}));
}

void TestScEtc::testLightFrame()
{
    QCOMPARE(sc_etc::lightFrame(sc_etc::Light::Red),       bytes({0x0A, 0x36, 0x0D}));
    QCOMPARE(sc_etc::lightFrame(sc_etc::Light::Green),     bytes({0x0A, 0x37, 0x0D}));
    QCOMPARE(sc_etc::lightFrame(sc_etc::Light::YellowOn),  bytes({0x0A, 0x38, 0x0D}));
    QCOMPARE(sc_etc::lightFrame(sc_etc::Light::YellowOff), bytes({0x0A, 0x39, 0x0D}));
}

void TestScEtc::testBrightnessFrame()
{
    // 亮度 7：0A 40 07 00 0D；越界夹紧到 0~7。
    QCOMPARE(sc_etc::brightnessFrame(7), bytes({0x0A, 0x40, 0x07, 0x00, 0x0D}));
    QCOMPARE(sc_etc::brightnessFrame(0), bytes({0x0A, 0x40, 0x00, 0x00, 0x0D}));
    QCOMPARE(sc_etc::brightnessFrame(9), bytes({0x0A, 0x40, 0x07, 0x00, 0x0D}));
}

void TestScEtc::testHeartbeatFrame()
{
    QCOMPARE(sc_etc::heartbeatFrame(), bytes({0x0A, 0x50, 0x0D}));
}

void TestScEtc::testAckScanner()
{
    sc_etc::AckScanner scanner;
    scanner.feed(QByteArray::fromHex("0a00"));
    sc_etc::AckReply r;
    QVERIFY(!scanner.next(&r)); // 帧尾未到

    scanner.feed(QByteArray::fromHex("0d0a010d0a020d"));
    QVERIFY(scanner.next(&r));
    QVERIFY(r.valid);
    QCOMPARE(r.kind, sc_etc::AckReply::Ok);
    QVERIFY(scanner.next(&r));
    QVERIFY(r.valid);
    QCOMPARE(r.kind, sc_etc::AckReply::TooLong);
    QVERIFY(scanner.next(&r));
    QVERIFY(r.valid);
    QCOMPARE(r.kind, sc_etc::AckReply::FrameError);
}

void TestScEtc::testAckScannerResync()
{
    // 前导杂散字节 + 正常应答。
    sc_etc::AckScanner scanner;
    scanner.feed(QByteArray::fromHex("ff0a000d"));
    sc_etc::AckReply r;
    QVERIFY(scanner.next(&r));
    QVERIFY(r.valid);
    QCOMPARE(r.kind, sc_etc::AckReply::Ok);
}

int runTestScEtc(int argc, char** argv)
{
    TestScEtc test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_sc_etc.moc"