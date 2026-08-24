#include <QtTest>

#include "protocol/yunnan/YunNanProtocol.h"

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

class TestYunNan : public QObject {
    Q_OBJECT

private slots:
    void testBuildFrameEnvelope();
    void testFullScreenLightPayload();
    void testOneLinePayload();
    void testFullScreenEditPayload();
    void testClearLinePayload();
    void testCivilVoicePayload();
    void testBrightnessPayload();
    void testVolumePayload();
    void testPeripheralPayload();
    void testFeeVoicePayload();
    void testParseQueryReply();
    void testParseVersionReply();
    void testGbkRoundTrip();
    void testFrameParserIncremental();
    void testFrameParserResync();
    void testFrameParserBinaryCmd();
};

void TestYunNan::testBuildFrameEnvelope()
{
    // '5' 全屏清除: 7B 35 00 7D.
    QCOMPARE(yunnan::buildFrame(yunnan::Cmd::ClearAll, QByteArray()),
             bytes({0x7B, '5', 0x00, 0x7D}));

    // 0x01 全屏点亮红色: 7B 01 01 01 7D.
    QCOMPARE(yunnan::buildFrame(yunnan::Cmd::FullScreenLight,
                                yunnan::fullScreenLightPayload(1)),
             bytes({0x7B, 0x01, 0x01, 0x01, 0x7D}));

    // 0x02 获取版本号: 7B 02 01 00 7D.
    QCOMPARE(yunnan::buildFrame(yunnan::Cmd::GetVersion, yunnan::versionPayload()),
             bytes({0x7B, 0x02, 0x01, 0x00, 0x7D}));
}

void TestYunNan::testFullScreenLightPayload()
{
    // 协议原文示例: 全屏红色 01 / 绿色 02 / 黄色 03.
    QCOMPARE(yunnan::fullScreenLightPayload(1), bytes({0x01}));
    QCOMPARE(yunnan::fullScreenLightPayload(2), bytes({0x02}));
    QCOMPARE(yunnan::fullScreenLightPayload(3), bytes({0x03}));
    // 设备扩展: 07 白.
    QCOMPARE(yunnan::fullScreenLightPayload(7), bytes({0x07}));
    // 越界钳制 1~7.
    QCOMPARE(yunnan::fullScreenLightPayload(0), bytes({0x01}));
    QCOMPARE(yunnan::fullScreenLightPayload(9), bytes({0x07}));
}

void TestYunNan::testOneLinePayload()
{
    // 协议原文示例 1: color '0'(红), row '1', "A".
    QCOMPARE(yunnan::oneLinePayload(0, 1, QStringLiteral("A")),
             bytes({0x30, 0x31, 'A'}));
    // 协议原文示例 2: color '1'(绿), row '2', "ETC车道" (GBK: ETC=B3B5, 道=B5C0).
    QCOMPARE(yunnan::oneLinePayload(1, 2, QStringLiteral("ETC车道")),
             bytes({0x31, 0x32, 'E', 'T', 'C', 0xB3, 0xB5, 0xB5, 0xC0}));
}

void TestYunNan::testFullScreenEditPayload()
{
    // 协议原文示例: color '0', x=0, y=44(0x2C), "ETC".
    QCOMPARE(yunnan::fullScreenEditPayload(0, 0, 44, QStringLiteral("ETC")),
             bytes({0x30, 0x00, 0x2C, 'E', 'T', 'C'}));
}

void TestYunNan::testClearLinePayload()
{
    QCOMPARE(yunnan::clearLinePayload(1), bytes({0x31}));
    QCOMPARE(yunnan::clearLinePayload(5), bytes({0x35}));
    QCOMPARE(yunnan::clearLinePayload(9), bytes({0x35})); // clamp to 5
}

void TestYunNan::testCivilVoicePayload()
{
    QCOMPARE(yunnan::civilVoicePayload(0), bytes({0x30}));
    QCOMPARE(yunnan::civilVoicePayload(3), bytes({0x33}));
    QCOMPARE(yunnan::civilVoicePayload(9), bytes({0x33})); // clamp to 3
}

void TestYunNan::testBrightnessPayload()
{
    // 自动档 = 0x00 (NUL), not ASCII '0'.
    QCOMPARE(yunnan::brightnessPayload(0), bytes({0x00}));
    // 手动档 1~8 → ASCII '1'~'8'（8 最亮）.
    QCOMPARE(yunnan::brightnessPayload(1), bytes({0x31}));
    QCOMPARE(yunnan::brightnessPayload(8), bytes({0x38}));
    QCOMPARE(yunnan::brightnessPayload(9), bytes({0x38})); // clamp to 8
}

void TestYunNan::testVolumePayload()
{
    QCOMPARE(yunnan::volumePayload(1), bytes({0x31}));
    QCOMPARE(yunnan::volumePayload(5), bytes({0x35}));
    QCOMPARE(yunnan::volumePayload(9), bytes({0x35})); // clamp to 5
}

void TestYunNan::testPeripheralPayload()
{
    // bit0 绿灯 = 0x01.
    QCOMPARE(yunnan::peripheralPayload(true, false, false), bytes({0x01}));
    // 协议原文示例 2: 绿灯关, 红灯开, 报警开 = 0x06.
    QCOMPARE(yunnan::peripheralPayload(false, true, true), bytes({0x06}));
    // 全开 = 0x07.
    QCOMPARE(yunnan::peripheralPayload(true, true, true), bytes({0x07}));
}

void TestYunNan::testFeeVoicePayload()
{
    // 协议原文示例: 7B 42 03 31 32 33 7D → "123".
    QCOMPARE(yunnan::feeVoicePayload(QStringLiteral("123")), bytes({'1', '2', '3'}));
    // 7B 42 05 31 32 33 2E 34 7D → "123.4".
    QCOMPARE(yunnan::feeVoicePayload(QStringLiteral("123.4")),
             bytes({'1', '2', '3', '.', '4'}));
    // 0 元 → 发送 "0"（设备不播报）.
    QCOMPARE(yunnan::feeVoicePayload(QStringLiteral("0")), bytes({'0'}));
    // 非法 → 回退 "0".
    QCOMPARE(yunnan::feeVoicePayload(QStringLiteral("abc")), bytes({'0'}));
}

void TestYunNan::testParseQueryReply()
{
    const yunnan::QueryReply normal =
        yunnan::parseQueryReply(QByteArray::fromHex("7b3101007d"));
    QVERIFY(normal.ok);
    QVERIFY(normal.normal);

    // Wire format reserves 0x01 = abnormal (device always replies normal 00).
    const yunnan::QueryReply abnormal =
        yunnan::parseQueryReply(QByteArray::fromHex("7b3101017d"));
    QVERIFY(abnormal.ok);
    QVERIFY(!abnormal.normal);

    // A raw data field is also accepted.
    const yunnan::QueryReply raw = yunnan::parseQueryReply(QByteArray::fromHex("00"));
    QVERIFY(raw.ok);
    QVERIFY(raw.normal);

    QVERIFY(!yunnan::parseQueryReply(QByteArray::fromHex("7b3201007d")).ok);
}

void TestYunNan::testParseVersionReply()
{
    // 裸 ASCII 文本（设备实际应答形态）.
    QCOMPARE(yunnan::parseVersionReply(QByteArray("YN_FX_P5_1.0")),
             QStringLiteral("YN_FX_P5_1.0"));
    // 带封套的版本号应答 7B 02 len ... 7D 也接受（"YN_FX_P5_1.0" 共 12 字节 → len 0x0C）.
    QCOMPARE(yunnan::parseVersionReply(QByteArray::fromHex("7b020c594e5f46585f50355f312e307d")),
             QStringLiteral("YN_FX_P5_1.0"));
    // 非 0x02 封套 → 空串.
    QVERIFY(yunnan::parseVersionReply(QByteArray::fromHex("7b3101007d")).isEmpty());
}

void TestYunNan::testGbkRoundTrip()
{
    // 云 = 0xD4C6, 南 = 0xC4CF (GB2312, GBK 子集).
    const QByteArray gbk = yunnan::toGbk(QStringLiteral("云南"));
    QCOMPARE(gbk, QByteArray::fromHex("d4c6c4cf"));
    QCOMPARE(yunnan::fromGbk(gbk), QStringLiteral("云南"));
}

void TestYunNan::testFrameParserIncremental()
{
    yunnan::FrameParser parser;

    parser.feed(QByteArray::fromHex("7b3801"));
    yunnan::Frame frame;
    QVERIFY(!parser.next(&frame)); // header + cmd + len, no data yet
    QVERIFY(parser.hasPending());

    parser.feed(QByteArray::fromHex("33"));
    QVERIFY(!parser.next(&frame)); // still missing the tail

    parser.feed(QByteArray::fromHex("7d"));
    QVERIFY(parser.next(&frame));
    QVERIFY(frame.ok);
    QCOMPARE(static_cast<char>(frame.cmd), '8');
    QCOMPARE(frame.data, QByteArray::fromHex("33"));
    QVERIFY(!parser.hasPending());
}

void TestYunNan::testFrameParserResync()
{
    yunnan::FrameParser parser;
    parser.feed(QByteArray::fromHex("ff7b3101007d"));

    yunnan::Frame frame;
    QVERIFY(parser.next(&frame));
    QVERIFY(frame.ok);
    QCOMPARE(static_cast<char>(frame.cmd), '1');
    QCOMPARE(frame.data, QByteArray::fromHex("00"));
}

void TestYunNan::testFrameParserBinaryCmd()
{
    yunnan::FrameParser parser;
    parser.feed(QByteArray::fromHex("7b0101017d"));

    yunnan::Frame frame;
    QVERIFY(parser.next(&frame));
    QVERIFY(frame.ok);
    QCOMPARE(static_cast<char>(frame.cmd), static_cast<char>(0x01));
    QCOMPARE(frame.data, QByteArray::fromHex("01"));

    parser.feed(QByteArray::fromHex("7b0201007d"));
    QVERIFY(parser.next(&frame));
    QVERIFY(frame.ok);
    QCOMPARE(static_cast<char>(frame.cmd), static_cast<char>(0x02));
    QCOMPARE(frame.data, QByteArray::fromHex("00"));
}

int runTestYunNan(int argc, char** argv)
{
    TestYunNan test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_yunnan.moc"