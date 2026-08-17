#include <QtTest>

#include "protocol/shandong/ShanDongProtocol.h"

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

class TestShanDong : public QObject {
    Q_OBJECT

private slots:
    void testBuildFrameEnvelope();
    void testFillAllFrame();
    void testVersionFrame();
    void testOneLineFrame();
    void testFullScreenFrame();
    void testFullScreenNewlines();
    void testClearFrame();
    void testBrightnessFrame();
    void testPeripheralFrame();
    void testReplyScanner();
};

void TestShanDong::testBuildFrameEnvelope()
{
    // '{' + '3' + len(2) + "AB" + '}'：长度字段为二进制值。
    const QByteArray f = shandong::buildFrame(shandong::Cmd::OneLine, QByteArray("AB"));
    QCOMPARE(f, bytes({0x7B, '3', 0x02, 'A', 'B', 0x7D}));
    // 长度字段 = 参数长度（非帧总长）。
    QCOMPARE(static_cast<quint8>(f.at(2)), static_cast<quint8>(2));
    QCOMPARE(f.size(), 6);

    // 超长参数夹紧到 255：len 字段 = 0xFF。
    const QByteArray big = shandong::buildFrame(shandong::Cmd::FullScreen,
                                                QByteArray(300, 'A'));
    QCOMPARE(big.size(), 259); // '{' + cmd + len + 255 + '}'
    QCOMPARE(static_cast<quint8>(big.at(2)), static_cast<quint8>(0xFF));
    QCOMPARE(big.at(big.size() - 1), static_cast<char>(0x7D));
}

void TestShanDong::testFillAllFrame()
{
    // '1' 参数为二进制值：01红/02绿/03黄。
    QCOMPARE(shandong::fillAllFrame(1), bytes({0x7B, '1', 0x01, 0x01, 0x7D}));
    QCOMPARE(shandong::fillAllFrame(2), bytes({0x7B, '1', 0x01, 0x02, 0x7D}));
    QCOMPARE(shandong::fillAllFrame(3), bytes({0x7B, '1', 0x01, 0x03, 0x7D}));
    // 越界夹紧。
    QCOMPARE(shandong::fillAllFrame(9), bytes({0x7B, '1', 0x01, 0x03, 0x7D}));
    QCOMPARE(shandong::fillAllFrame(0), bytes({0x7B, '1', 0x01, 0x01, 0x7D}));
}

void TestShanDong::testVersionFrame()
{
    // 协议示例：7B 32 01 00 7D。
    QCOMPARE(shandong::versionFrame(), bytes({0x7B, '2', 0x01, 0x00, 0x7D}));
}

void TestShanDong::testOneLineFrame()
{
    // '3' 单行示例1：7B 33 03 30 31 41 7D（颜色'0' + 行号'1' + "A"）。
    QCOMPARE(shandong::oneLineFrame(0, 1, QStringLiteral("A")),
             bytes({0x7B, '3', 0x03, 0x30, 0x31, 'A', 0x7D}));

    // '3' 单行示例2（ETC车道，GBK）：7B 33 09 31 32 45 54 43 B3 B5 B5 C0 7D。
    const QByteArray f = shandong::oneLineFrame(1, 2, QStringLiteral("ETC车道"));
    QCOMPARE(f, bytes({0x7B, '3', 0x09, 0x31, 0x32, 0x45, 0x54, 0x43,
                       0xB3, 0xB5, 0xB5, 0xC0, 0x7D}));
    // 长度字段 = 2 + GBK 文本长度。
    QCOMPARE(static_cast<quint8>(f.at(2)), static_cast<quint8>(9));

    // 行号/颜色越界夹紧。
    QCOMPARE(shandong::oneLineFrame(7, 9, QStringLiteral("AB")),
             bytes({0x7B, '3', 0x04, 0x32, 0x35, 'A', 'B', 0x7D}));
}

void TestShanDong::testFullScreenFrame()
{
    // '4' 全屏示例（ETC车道已关闭）：7B 34 10 30 00 2C 45 54 43 B3 B5 B5 C0
    //                                   D2 D1 B9 D8 B1 D5 7D。
    const QByteArray f =
        shandong::fullScreenFrame(0, 0x00, 0x2C, QStringLiteral("ETC车道已关闭"));
    QCOMPARE(f, bytes({0x7B, '4', 0x10, 0x30, 0x00, 0x2C, 0x45, 0x54, 0x43,
                       0xB3, 0xB5, 0xB5, 0xC0, 0xD2, 0xD1, 0xB9, 0xD8, 0xB1, 0xD5, 0x7D}));
    // 长度字段 = 3 + GBK 文本长度。
    QCOMPARE(static_cast<quint8>(f.at(2)), static_cast<quint8>(0x10));
}

void TestShanDong::testFullScreenNewlines()
{
    // "\n" → 0x0A；参数 = 颜色 + X + Y + "A" 0x0A "B"。
    QCOMPARE(shandong::fullScreenFrame(0, 0x00, 0x00, QStringLiteral("A\nB")),
             bytes({0x7B, '4', 0x06, 0x30, 0x00, 0x00, 'A', 0x0A, 'B', 0x7D}));

    // "\r\n" → 0x0D 0x0A（0x0D 被渲染引擎跳过、0x0A 换行）。
    QCOMPARE(shandong::fullScreenFrame(0, 0x00, 0x00, QStringLiteral("A\r\nB")),
             bytes({0x7B, '4', 0x07, 0x30, 0x00, 0x00, 'A', 0x0D, 0x0A, 'B', 0x7D}));
}

void TestShanDong::testClearFrame()
{
    // '5' 无参数：7B 35 00 7D。
    QCOMPARE(shandong::clearFrame(), bytes({0x7B, '5', 0x00, 0x7D}));
}

void TestShanDong::testBrightnessFrame()
{
    // '7'：7B 37 01 30 7D（'0' 自动）。
    QCOMPARE(shandong::brightnessFrame(0), bytes({0x7B, '7', 0x01, 0x30, 0x7D}));
    QCOMPARE(shandong::brightnessFrame(5), bytes({0x7B, '7', 0x01, 0x35, 0x7D}));
    // 越界夹紧（'6' 非法 → 夹到 '5'）。
    QCOMPARE(shandong::brightnessFrame(6), bytes({0x7B, '7', 0x01, 0x35, 0x7D}));
}

void TestShanDong::testPeripheralFrame()
{
    // '8' 位图：bit0 绿 / bit1 红 / bit2 黄闪。
    QCOMPARE(shandong::peripheralFrame(true, false, false),
             bytes({0x7B, '8', 0x01, 0x01, 0x7D}));
    QCOMPARE(shandong::peripheralFrame(false, true, false),
             bytes({0x7B, '8', 0x01, 0x02, 0x7D}));
    QCOMPARE(shandong::peripheralFrame(false, false, true),
             bytes({0x7B, '8', 0x01, 0x04, 0x7D}));
    QCOMPARE(shandong::peripheralFrame(false, true, true),
             bytes({0x7B, '8', 0x01, 0x06, 0x7D}));
    QCOMPARE(shandong::peripheralFrame(true, true, true),
             bytes({0x7B, '8', 0x01, 0x07, 0x7D}));
    QCOMPARE(shandong::peripheralFrame(false, false, false),
             bytes({0x7B, '8', 0x01, 0x00, 0x7D}));
}

void TestShanDong::testReplyScanner()
{
    // 版本号裸 ASCII 应答 + 非可打印终止字节。
    shandong::ReplyScanner scanner;
    scanner.feed(QByteArray("9K10212482"));
    scanner.feed(QByteArray(1, static_cast<char>(0x00)));
    shandong::Reply r;
    QVERIFY(scanner.next(&r));
    QVERIFY(r.valid);
    QCOMPARE(r.text, QStringLiteral("9K10212482"));

    // 无终止字节的文本应答（一次性完整到达）：靠 flush 兜底。
    shandong::ReplyScanner scanner2;
    scanner2.feed(QByteArray("9K10212482"));
    QVERIFY(!scanner2.next(&r)); // 串未终止，等待
    QVERIFY(scanner2.flush(&r));
    QVERIFY(r.valid);
    QCOMPARE(r.text, QStringLiteral("9K10212482"));

    // 短杂散字节 flush 无效并清空。
    shandong::ReplyScanner scanner3;
    scanner3.feed(QByteArray("abc"));
    QVERIFY(!scanner3.flush(&r));

    // 不可打印杂散字节丢弃后继续扫描。
    shandong::ReplyScanner scanner4;
    scanner4.feed(QByteArray::fromHex("ff0a")); // 垃圾 + 0x0A
    scanner4.feed(QByteArray("9K10212482"));
    scanner4.feed(QByteArray(1, static_cast<char>(0x00)));
    QVERIFY(scanner4.next(&r));
    QVERIFY(r.valid);
    QCOMPARE(r.text, QStringLiteral("9K10212482"));
}

int runTestShanDong(int argc, char** argv)
{
    TestShanDong test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_shandong.moc"