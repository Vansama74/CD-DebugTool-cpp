#include <QtTest>

#include "protocol/sichuan_mtc/SiChuanMtcProtocol.h"

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

class TestScMtc : public QObject {
    Q_OBJECT

private slots:
    void testBracesFrameBcc();
    void testInitSelfCheckClear();
    void testOneLineFrame();
    void testFullScreenFrame();
    void testFixedBusFrame();
    void testFixedTruckFrame();
    void testVoiceFrames();
    void testByteValFrames();
    void testRawFamily();
    void testHostFamily();
    void testWeight5();
    void testReplyScanner();
};

void TestScMtc::testBracesFrameBcc()
{
    // {1}：无参数，BCC = '1'。帧 = 7B 31 31 7D
    QCOMPARE(sc_mtc::bracesFrame(sc_mtc::Cmd::Init, QByteArray()), bytes({0x7B, '1', '1', 0x7D}));
    // 不带 BCC 变体：7B 31 7D
    QCOMPARE(sc_mtc::bracesFrame(sc_mtc::Cmd::Init, QByteArray(), false),
             bytes({0x7B, '1', 0x7D}));
    // {8 5}：BCC = '8'^'5' = 0x0D
    QCOMPARE(sc_mtc::bracesFrame(sc_mtc::Cmd::Brightness, QByteArray("5")),
             bytes({0x7B, '8', '5', 0x0D, 0x7D}));
}

void TestScMtc::testInitSelfCheckClear()
{
    QCOMPARE(sc_mtc::initFrame(),      bytes({0x7B, '1', '1', 0x7D}));
    QCOMPARE(sc_mtc::selfCheckFrame(), bytes({0x7B, '2', '2', 0x7D}));
    QCOMPARE(sc_mtc::clearFrame(),     bytes({0x7B, '5', '5', 0x7D}));
}

void TestScMtc::testOneLineFrame()
{
    // {3 1 <16B> BCC}：文本 "AB" 后补 14 个空格。
    QByteArray expected = QByteArray::fromHex("7b33");
    expected.append('1');
    expected.append("AB");
    expected.append(14, static_cast<char>(0x20));
    // BCC = '3' ^ 全部 17 参数字节
    char bcc = '3';
    for (char c : expected.mid(2))
        bcc ^= c;
    expected.append(bcc);
    expected.append(static_cast<char>(0x7D));

    QCOMPARE(sc_mtc::oneLineFrame(1, QStringLiteral("AB")), expected);
    QCOMPARE(sc_mtc::oneLineFrame(1, QStringLiteral("AB")).size(), 21); // 7B 33 +17 +BCC +7D
}

void TestScMtc::testFullScreenFrame()
{
    // {4 <64B> BCC}：总长 67 + BCC = 68 字节。
    const QByteArray frame = sc_mtc::fullScreenFrame(QStringLiteral("四川"));
    QCOMPARE(frame.size(), 68);
    QCOMPARE(frame.at(0), static_cast<char>(0x7B));
    QCOMPARE(frame.at(1), '4');
    QCOMPARE(frame.at(frame.size() - 1), static_cast<char>(0x7D));

    // 数据区 64B：GBK "四川" (CB C4 B4 A8) + 60 空格。
    QCOMPARE(frame.mid(2, 64),
             QByteArray::fromHex("cbc4b4a8") + QByteArray(60, static_cast<char>(0x20)));
}

void TestScMtc::testFixedBusFrame()
{
    // 客车 type=0，车型 1，金额 5，余额 20：X0~X11 = "010000500020"
    const QByteArray frame = sc_mtc::fixedBusFrame(1, 5, 20);
    QCOMPARE(frame.at(0), static_cast<char>(0x7B));
    QCOMPARE(frame.at(1), '6');
    QCOMPARE(frame.mid(2, 12), QByteArray("010000500020"));
    QCOMPARE(frame.size(), 16); // 7B 36 + 12 + BCC + 7D
}

void TestScMtc::testFixedTruckFrame()
{
    // 货车 type=1：X1~X5 总重 12.34 → "01234"；金额 55；余额 100；超重 1.2 → "00120"
    const QByteArray frame = sc_mtc::fixedTruckFrame(QStringLiteral("12.34"), 55, 100,
                                                     QStringLiteral("1.2"));
    QCOMPARE(frame.mid(2, 21), QByteArray("101234000550010000120"));
    QCOMPARE(frame.size(), 25); // 7B 36 + 21 + BCC + 7D
}

void TestScMtc::testVoiceFrames()
{
    // {7 5}：BCC = '7'^'5' = 0x02
    QCOMPARE(sc_mtc::voiceFrame(5), bytes({0x7B, '7', '5', 0x02, 0x7D}));
    // 自定义语音：{7 8 <GBK 文本> BCC}
    const QByteArray custom = sc_mtc::customVoiceFrame(QStringLiteral("川"));
    QCOMPARE(custom.mid(0, 3), QByteArray::fromHex("7b3738"));
    QCOMPARE(custom.mid(3, 2), QByteArray::fromHex("b4a8")); // 川 GBK
    QCOMPARE(custom.at(custom.size() - 1), static_cast<char>(0x7D));
}

void TestScMtc::testByteValFrames()
{
    // {8 0}：BCC = '8'^'0' = 0x08
    QCOMPARE(sc_mtc::brightnessFrame(0), bytes({0x7B, '8', '0', 0x08, 0x7D}));
    // {9 5}：BCC = '9'^'5' = 0x0C
    QCOMPARE(sc_mtc::volumeFrame(5), bytes({0x7B, '9', '5', 0x0C, 0x7D}));
    // {A 1}：BCC = 'A'^'1' = 0x70
    QCOMPARE(sc_mtc::colorFrame(1), bytes({0x7B, 'A', '1', 0x70, 0x7D}));
}

void TestScMtc::testRawFamily()
{
    // 文档示例：9600 = 7B 40 00 25 80 7D；115200 = 7B 40 01 C2 00 7D
    QCOMPARE(sc_mtc::rawBaudFrame(0), QByteArray::fromHex("7b400025807d"));
    QCOMPARE(sc_mtc::rawBaudFrame(1), QByteArray::fromHex("7b4001c2007d"));

    QCOMPARE(sc_mtc::rawDotSizeFrame(2), QByteArray::fromHex("7b41027d"));
    QCOMPARE(sc_mtc::rawFontFrame(3),    QByteArray::fromHex("7b42037d"));
    QCOMPARE(sc_mtc::rawProtoFrame(1),   QByteArray::fromHex("7b43017d"));
    QCOMPARE(sc_mtc::rawFillAllFrame(0), QByteArray::fromHex("7b44007d"));
    QCOMPARE(sc_mtc::rawVersionFrame(),  QByteArray::fromHex("7b457d"));
}

void TestScMtc::testHostFamily()
{
    QCOMPARE(sc_mtc::hostQueryFrame(), QByteArray::fromHex("0a460a"));
    QCOMPARE(sc_mtc::hostClearFrame(), QByteArray::fromHex("0a460d"));
}

void TestScMtc::testWeight5()
{
    QCOMPARE(sc_mtc::weight5(QStringLiteral("12.34")), QByteArray("01234"));
    QCOMPARE(sc_mtc::weight5(QStringLiteral("0.5")),   QByteArray("00050"));
    QCOMPARE(sc_mtc::weight5(QStringLiteral("999")),   QByteArray("99900"));
    QCOMPARE(sc_mtc::weight5(QStringLiteral("1234")),  QByteArray("99900")); // 越界夹紧
    QCOMPARE(sc_mtc::weight5(QStringLiteral("")),      QByteArray("00000"));
}

void TestScMtc::testReplyScanner()
{
    // 主机查询应答 0A 64 0A（正常）
    sc_mtc::ReplyScanner scanner;
    scanner.feed(QByteArray::fromHex("0a640a"));
    sc_mtc::Reply r;
    QVERIFY(scanner.next(&r));
    QVERIFY(r.valid);
    QCOMPARE(r.kind, sc_mtc::Reply::HostQuery);
    QVERIFY(r.hostNormal);

    // 版本号文本应答
    scanner.feed(QByteArray("SC_FX_P7.62_1.0"));
    scanner.feed(QByteArray(1, static_cast<char>(0x00))); // 文本后非可打印字节结束串
    QVERIFY(scanner.next(&r));
    QVERIFY(r.valid);
    QCOMPARE(r.kind, sc_mtc::Reply::Text);
    QCOMPARE(r.text, QStringLiteral("SC_FX_P7.62_1.0"));

    // 无终止字节的文本应答（一次性完整到达）：靠 flush 兜底。
    sc_mtc::ReplyScanner scanner2;
    scanner2.feed(QByteArray("SC_FX_P7.62_1.0"));
    QVERIFY(!scanner2.next(&r)); // 串未终止，等待
    QVERIFY(scanner2.flush(&r));
    QVERIFY(r.valid);
    QCOMPARE(r.kind, sc_mtc::Reply::Text);
    QCOMPARE(r.text, QStringLiteral("SC_FX_P7.62_1.0"));

    // 短杂散字节 flush 无效并清空。
    sc_mtc::ReplyScanner scanner3;
    scanner3.feed(QByteArray("abc"));
    QVERIFY(!scanner3.flush(&r));
}

int runTestScMtc(int argc, char** argv)
{
    TestScMtc test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_sc_mtc.moc"