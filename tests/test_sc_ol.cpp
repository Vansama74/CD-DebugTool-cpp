#include <QtTest>

#include "protocol/sichuan_ol/SiChuanOlProtocol.h"

class TestScOl : public QObject {
    Q_OBJECT

private slots:
    void testClearFrame();
    void testQueryFrames();
    void testBrightnessFrame();
    void testLaneAndFlashFrames();
    void testLineFrame();
    void testFullScreenFrame();
    void testBcc();
    void testFrameParserIncremental();
    void testFrameParserResync();
    void testParseContentReply();
    void testParseStatusReply();
};

void TestScOl::testClearFrame()
{
    // 文档示例：FF 07 94 00 00 6C FF
    QCOMPARE(sc_ol::clearFrame(), QByteArray::fromHex("ff079400006cff"));
}

void TestScOl::testQueryFrames()
{
    // 文档示例：A0/B6/B9/B8 查询帧，BCC 分别为 58/4E/41/40
    QCOMPARE(sc_ol::queryContentFrame(), QByteArray::fromHex("ff07a0000058ff"));
    QCOMPARE(sc_ol::queryBrightFrame(),  QByteArray::fromHex("ff07b600004eff"));
    QCOMPARE(sc_ol::queryLaneFrame(),    QByteArray::fromHex("ff07b9000041ff"));
    QCOMPARE(sc_ol::queryFlashFrame(),   QByteArray::fromHex("ff07b8000040ff"));
}

void TestScOl::testBrightnessFrame()
{
    // 亮度 0xFF：BCC = FF^07^96^00^FF = 91
    QCOMPARE(sc_ol::brightnessFrame(0xFF), QByteArray::fromHex("ff079600ff91ff"));
    // 亮度 0x00（自动调光）：BCC = FF^07^96^00^00 = 6E
    QCOMPARE(sc_ol::brightnessFrame(0x00), QByteArray::fromHex("ff079600006eff"));
}

void TestScOl::testLaneAndFlashFrames()
{
    // 通行灯绿（01）：BCC = FF^07^99^00^01 = 60
    QCOMPARE(sc_ol::laneLightFrame(true), QByteArray::fromHex("ff0799000160ff"));
    // 通行灯红（00）：BCC = FF^07^99^00^00 = 61
    QCOMPARE(sc_ol::laneLightFrame(false), QByteArray::fromHex("ff0799000061ff"));
    // 黄闪开（01）：BCC = FF^07^98^00^01 = 61
    QCOMPARE(sc_ol::yellowFlashFrame(true), QByteArray::fromHex("ff0798000161ff"));
    // 黄闪关（00）：BCC = FF^07^98^00^00 = 60
    QCOMPARE(sc_ol::yellowFlashFrame(false), QByteArray::fromHex("ff0798000060ff"));
}

void TestScOl::testLineFrame()
{
    // 第 1 行显示 "AB"：FF 16 81 FF <16B 数据(AB+14 空格)> BCC FF
    const QByteArray frame = sc_ol::lineFrame(0, QStringLiteral("AB"));
    QCOMPARE(frame.size(), 22);
    QCOMPARE(frame.mid(0, 4), QByteArray::fromHex("ff1681ff"));
    QCOMPARE(frame.mid(4, 16),
             QByteArray("AB") + QByteArray(14, static_cast<char>(0x20)));
    QCOMPARE(static_cast<quint8>(frame.at(20)), sc_ol::bcc(frame));
    QCOMPARE(static_cast<quint8>(frame.at(21)), 0xFF);

    // 行号越界夹紧：第 9 行 → 第 8 行 (88)
    QCOMPARE(static_cast<quint8>(sc_ol::lineFrame(9, QStringLiteral("A")).at(2)), 0x88);
}

void TestScOl::testFullScreenFrame()
{
    // 全屏 "AB"：FF 08 80 FF 41 42 BCC FF
    const QByteArray frame = sc_ol::fullScreenFrame(QStringLiteral("AB"));
    QCOMPARE(frame.size(), 8);
    QCOMPARE(frame.mid(0, 4), QByteArray::fromHex("ff0880ff"));
    QCOMPARE(frame.mid(4, 2), QByteArray("AB"));
    QCOMPARE(static_cast<quint8>(frame.at(6)), sc_ol::bcc(frame));
}

void TestScOl::testBcc()
{
    // 清屏帧五字段异或：FF^07^94^00^00 = 6C
    QCOMPARE(sc_ol::bcc(QByteArray::fromHex("ff079400006cff")), 0x6C);
}

void TestScOl::testFrameParserIncremental()
{
    sc_ol::FrameParser parser;
    parser.feed(QByteArray::fromHex("ff079400006c"));
    sc_ol::Frame frame;
    QVERIFY(!parser.next(&frame)); // 尾部 FF 未到

    parser.feed(QByteArray::fromHex("ff"));
    QVERIFY(parser.next(&frame));
    QVERIFY(frame.ok);
    QVERIFY(frame.bccOk);
    QCOMPARE(frame.cmd, 0x94);
    QCOMPARE(frame.bright, 0x00);
    QCOMPARE(frame.data, QByteArray(1, static_cast<char>(0x00))); // 短帧数据段恒 1 字节 00
}

void TestScOl::testFrameParserResync()
{
    // 前导杂散字节 + 完整查询帧。
    sc_ol::FrameParser parser;
    parser.feed(QByteArray::fromHex("a5a5ff07b600004eff"));
    sc_ol::Frame frame;
    QVERIFY(parser.next(&frame));
    QVERIFY(frame.ok);
    QCOMPARE(frame.cmd, 0xB6);
}

void TestScOl::testParseContentReply()
{
    // 模拟设备 A1 行应答：FF 16 A1 FF <"AB"+14 空格> BCC FF
    QByteArray data = QByteArray("AB");
    data.append(14, static_cast<char>(0x20));
    const QByteArray frame = sc_ol::buildFrame(0xA1, 0xFF, data);

    sc_ol::Frame parsed;
    QVERIFY(sc_ol::bcc(frame) == static_cast<quint8>(frame.at(frame.size() - 2)));

    // 直接构造 Frame 结构
    parsed.cmd    = 0xA1;
    parsed.bright = 0xFF;
    parsed.data   = data;
    parsed.bccOk  = true;
    parsed.ok     = true;

    const sc_ol::ContentReply reply = sc_ol::parseContentReply(parsed);
    QVERIFY(reply.ok);
    QCOMPARE(reply.row, 0);
    QCOMPARE(reply.text, QByteArray("AB")); // 尾部补空格已去除
    QCOMPARE(reply.bright, 0xFF);
}

void TestScOl::testParseStatusReply()
{
    // B6 亮度应答（FF 07 B6 00 FF B1 FF）
    sc_ol::Frame parsed;
    parsed.cmd    = 0xB6;
    parsed.bright = 0x00;
    parsed.data   = QByteArray(1, static_cast<char>(0xFF));
    parsed.bccOk  = true;
    parsed.ok     = true;

    const sc_ol::StatusReply reply = sc_ol::parseStatusReply(parsed);
    QVERIFY(reply.ok);
    QCOMPARE(reply.value, 0xFF);

    // 非状态命令不解析
    parsed.cmd = 0x99;
    QVERIFY(!sc_ol::parseStatusReply(parsed).ok);
}

int runTestScOl(int argc, char** argv)
{
    TestScOl test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_sc_ol.moc"