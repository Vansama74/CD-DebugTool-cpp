#include <QtTest>

#include "protocol/qinghai/QingHaiProtocol.h"

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

class TestQingHai : public QObject {
    Q_OBJECT

private slots:
    void testBuildFrame();
    void testOneLinePayload();
    void testFullScreenPayload();
    void testFixedDisplayPayload();
    void testPeripheralPayload();
    void testFeeVoicePayload();
    void testParseQueryReply();
    void testGbkRoundTrip();
    void testFrameParserIncremental();
    void testFrameParserResync();
};

void TestQingHai::testBuildFrame()
{
    QCOMPARE(qinghai::buildFrame(qinghai::Cmd::Clear, QByteArray()),
             bytes({0x7B, '5', 0x00, 0x7D}));

    QCOMPARE(qinghai::buildFrame(qinghai::Cmd::Brightness, qinghai::levelPayload(3)),
             bytes({0x7B, '8', 0x01, '3', 0x7D}));
}

void TestQingHai::testOneLinePayload()
{
    // color ASCII '1' (green), row ASCII '3', text ASCII.
    QCOMPARE(qinghai::oneLinePayload(1, 3, QStringLiteral("ETC")),
             bytes({0x31, 0x33, 'E', 'T', 'C'}));
}

void TestQingHai::testFullScreenPayload()
{
    // Matches the firmware spec example: color '0', x=0, y=44 (0x2C).
    QCOMPARE(qinghai::fullScreenPayload(0, 0, 44, QStringLiteral("ETC")),
             bytes({0x30, 0x00, 0x2C, 'E', 'T', 'C'}));
}

void TestQingHai::testFixedDisplayPayload()
{
    // type '1' (货车), then the '|'-separated fields verbatim (ASCII here).
    QCOMPARE(qinghai::fixedDisplayPayload(1, QStringLiteral("客车|5.00")),
             bytes({0x31, 0xBF, 0xCD, 0xB3, 0xB5, '|', '5', '.', '0', '0'}));
}

void TestQingHai::testPeripheralPayload()
{
    // green bit0 + yellow bit2 = 0x05.
    QCOMPARE(qinghai::peripheralPayload(true, false, true), bytes({0x05}));
    // red bit1 = 0x02.
    QCOMPARE(qinghai::peripheralPayload(false, true, false), bytes({0x02}));
    // all three = 0x07.
    QCOMPARE(qinghai::peripheralPayload(true, true, true), bytes({0x07}));
}

void TestQingHai::testFeeVoicePayload()
{
    // 500 fen -> "00500", prefixed by type '0'.
    QCOMPARE(qinghai::feeVoicePayload(0, QStringLiteral("500")),
             bytes({0x30, '0', '0', '5', '0', '0'}));
    // 123 yuan = 12300 fen -> "12300".
    QCOMPARE(qinghai::feeVoicePayload(0, QStringLiteral("12300")),
             bytes({0x30, '1', '2', '3', '0', '0'}));
    // yuan string with a decimal point is converted (5.00 yuan -> 500 fen).
    QCOMPARE(qinghai::feeVoicePayload(0, QStringLiteral("5.00")),
             bytes({0x30, '0', '0', '5', '0', '0'}));
}

void TestQingHai::testParseQueryReply()
{
    const qinghai::QueryReply normal =
        qinghai::parseQueryReply(QByteArray::fromHex("7b3101007d"));
    QVERIFY(normal.ok);
    QVERIFY(normal.normal);

    // Firmware never emits this, but the wire format reserves 0x01 = abnormal.
    const qinghai::QueryReply abnormal =
        qinghai::parseQueryReply(QByteArray::fromHex("7b3101017d"));
    QVERIFY(abnormal.ok);
    QVERIFY(!abnormal.normal);

    // A raw data field is also accepted.
    const qinghai::QueryReply raw = qinghai::parseQueryReply(QByteArray::fromHex("00"));
    QVERIFY(raw.ok);
    QVERIFY(raw.normal);

    QVERIFY(!qinghai::parseQueryReply(QByteArray::fromHex("7b3201007d")).ok);
}

void TestQingHai::testGbkRoundTrip()
{
    const QByteArray gbk = qinghai::toGbk(QStringLiteral("青海"));
    QVERIFY(!gbk.isEmpty());
    // 青 = 0xC7E0, 海 = 0xBAA3.
    QCOMPARE(gbk, QByteArray::fromHex("c7e0baa3"));
    QCOMPARE(qinghai::fromGbk(gbk), QStringLiteral("青海"));
}

void TestQingHai::testFrameParserIncremental()
{
    qinghai::FrameParser parser;

    parser.feed(QByteArray::fromHex("7b3801"));
    qinghai::Frame frame;
    QVERIFY(!parser.next(&frame)); // header + cmd + len, no data yet

    parser.feed(QByteArray::fromHex("33"));
    QVERIFY(!parser.next(&frame)); // still missing the tail

    parser.feed(QByteArray::fromHex("7d"));
    QVERIFY(parser.next(&frame));
    QVERIFY(frame.ok);
    QCOMPARE(static_cast<char>(frame.cmd), '8');
    QCOMPARE(frame.data, QByteArray::fromHex("33"));
}

void TestQingHai::testFrameParserResync()
{
    qinghai::FrameParser parser;
    parser.feed(QByteArray::fromHex("ff7b3101007d"));

    qinghai::Frame frame;
    QVERIFY(parser.next(&frame));
    QVERIFY(frame.ok);
    QCOMPARE(static_cast<char>(frame.cmd), '1');
    QCOMPARE(frame.data, QByteArray::fromHex("00"));
}

int runTestQingHai(int argc, char** argv)
{
    TestQingHai test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_qinghai.moc"
