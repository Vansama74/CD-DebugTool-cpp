#include <QtTest>

#include <QFile>

#include "protocol/iap/IntelHexParser.h"

class TestIntelHex : public QObject {
    Q_OBJECT

private slots:
    void testSimpleData();
    void testExtendedLinearAddress();
    void testExtendedLinearAddressHighBase();
    void testExtendedSegmentAddress();
    void testExtendedSegmentAddressRebase();
    void testObjcopyType02Hex();
    void testGapFill();
    void testSpanOverLimit();
    void testBadChecksum();
    void testMissingEof();
    void testUnknownRecordType();
};

void TestIntelHex::testSimpleData()
{
    // 一条数据记录（addr 0, 4 字节）+ EOF。校验和 0xF2：
    // sum(04 00 00 00 01 02 03 04)=0x0E → CC=0x100-0x0E=0xF2。
    const QByteArray hex = ":0400000001020304F2\r\n:00000001FF\r\n";
    QByteArray out;
    QString err;
    QVERIFY2(IntelHexParser::parse(hex, &out, &err), qPrintable(err));
    QCOMPARE(out, QByteArray::fromHex("01020304"));
}

void TestIntelHex::testExtendedLinearAddress()
{
    // 04 记录设置扩展线性地址 0x0001（base=0x10000）+ 偏移 0x0800 的数据记录。
    // 新语义（两遍裁剪）：输出紧凑区间 [minAbs, maxEnd) = [0x10800, 0x10804)，
    // 仅 4 字节，不再按绝对地址展开带 0x10800 前导。
    const QByteArray hex =
        ":020000040001F9\r\n" // sum=02+00+00+04+00+01=0x07 → CC=F9
        ":04080000112233444A\r\n" // sum=0xB6 → CC=4A
        ":00000001FF\r\n";
    QByteArray out;
    QString err;
    QVERIFY2(IntelHexParser::parse(hex, &out, &err), qPrintable(err));
    QCOMPARE(out.size(), 4);
    QCOMPARE(out, QByteArray::fromHex("11223344"));
}

void TestIntelHex::testExtendedLinearAddressHighBase()
{
    // 回归（134MB bug）：04 记录高基址（0x0804 → 0x08040000，固件 ELF hex
    // 典型基址）。旧实现按绝对地址 `out->resize(end)` 展开，末条记录
    // end=0x080793A4 会把输出膨胀到 134,714,276 字节；新语义裁剪到
    // [minAbs, maxEnd) 紧凑区间，输出仅覆盖记录本身的字节数。
    const QByteArray hex =
        ":020000040804EE\r\n" // 04 基址 0x0804：sum=02+00+00+04+08+04=0x12 → CC=EE
        ":04000000AABBCCDDEE\r\n" // sum=0x312 → CC=0x100-0x12=EE
        ":00000001FF\r\n";
    QByteArray out;
    QString err;
    QVERIFY2(IntelHexParser::parse(hex, &out, &err), qPrintable(err));
    QCOMPARE(out.size(), 4); // 不再膨胀为 0x08040004（134,742,020 字节）
    QCOMPARE(out, QByteArray::fromHex("AABBCCDD"));
}

void TestIntelHex::testExtendedSegmentAddress()
{
    // 02 记录（扩展段地址）：base = hi << 4（段地址 ×16），与 04（hi << 16）
    // 仅移位量不同。:020000020004F8 → sum=0x08 → CC=0xF8，base=0x0004<<4=0x40。
    // 后续数据记录 addr=0x0000 → abs=0x40；addr=0x0008 → abs=0x48（中间 4 字节
    // 间隙按 0xFF 填充）。输出紧凑区间 [0x40, 0x4C) 共 12 字节。
    const QByteArray hex =
        ":020000020004F8\r\n"      // 02: 段地址 0x0004 → base=0x40
        ":040000001122334452\r\n"  // 数据 abs=0x40（sum=0xAE → CC=0x52）
        ":04000800AABBCCDDE6\r\n"  // 数据 abs=0x48（CC=0xE6）
        ":00000001FF\r\n";
    QByteArray out;
    QString err;
    QVERIFY2(IntelHexParser::parse(hex, &out, &err), qPrintable(err));
    QCOMPARE(out.size(), 12);
    QCOMPARE(out.mid(0, 4), QByteArray::fromHex("11223344"));
    QCOMPARE(out.mid(4, 4), QByteArray::fromHex("FFFFFFFF"));
    QCOMPARE(out.mid(8, 4), QByteArray::fromHex("AABBCCDD"));
}

void TestIntelHex::testExtendedSegmentAddressRebase()
{
    // 两条 02 记录先后重定位段地址：0x0004 → base=0x40；0x1234 → base=0x12340。
    // 若误用 04 的 <<16 语义（base=0x12340000），输出跨度将远超 16 MiB 上限被
    // 拒——此处断言 02 的 base 是段地址 ×16，数据落在 0x12340 偏移。
    const QByteArray hex =
        ":020000020004F8\r\n"      // 02: base=0x40
        ":040000001122334452\r\n"  // 数据 abs=0x40
        ":020000021234B6\r\n"      // 02: 0x1234 → base=0x12340（sum=0x4A → CC=B6）
        ":04000000AABBCCDDEE\r\n"  // 数据 abs=0x12340（sum=0x112 → CC=EE）
        ":00000001FF\r\n";
    QByteArray out;
    QString err;
    QVERIFY2(IntelHexParser::parse(hex, &out, &err), qPrintable(err));
    QCOMPARE(out.size(), 0x12340 - 0x40 + 4); // 74500
    QCOMPARE(out.mid(0, 4), QByteArray::fromHex("11223344"));
    QCOMPARE(out.mid(0x12340 - 0x40, 4), QByteArray::fromHex("AABBCCDD"));
}

void TestIntelHex::testObjcopyType02Hex()
{
    // objcopy -I binary -O ihex 生成的固件 hex：GNU objcopy（本项目 toolchain
    // 2.45.1）以 02（扩展段地址）记录表达 64KiB 段边界（本 fixture 3 条 02），
    // 而非 04。fixture 由 Project_STD.bin（234404 字节）生成：
    //   arm-none-eabi-objcopy -I binary -O ihex \
    //       build/Debug/Project_STD.bin tests/data/t02_project_std.hex
    // 断言：解析 size 与 bin 一致（234404），且逐字节相同（bin→hex 无地址间隙，
    // 02 基址语义正确时二者应完全一致）。
    QFile hexFile(QStringLiteral(TEST_DATA_DIR "/t02_project_std.hex"));
    QVERIFY2(hexFile.open(QIODevice::ReadOnly), qPrintable(hexFile.errorString()));
    const QByteArray hex = hexFile.readAll();

    QByteArray out;
    QString err;
    QVERIFY2(IntelHexParser::parse(hex, &out, &err), qPrintable(err));
    QCOMPARE(out.size(), 234404);

    QFile binFile(QStringLiteral(TEST_DATA_DIR "/t02_project_std.bin"));
    QVERIFY2(binFile.open(QIODevice::ReadOnly), qPrintable(binFile.errorString()));
    QCOMPARE(out, binFile.readAll());
}

void TestIntelHex::testGapFill()
{
    // 两条数据记录中间留 4 字节间隙：间隙字节必须为 0xFF。
    const QByteArray hex =
        ":0400000001020304F2\r\n"
        ":04000800AABBCCDDE6\r\n" // sum=0x31A → CC=0x100-0x1A=E6
        ":00000001FF\r\n";
    QByteArray out;
    QString err;
    QVERIFY2(IntelHexParser::parse(hex, &out, &err), qPrintable(err));
    QCOMPARE(out.size(), 12);
    QCOMPARE(out.mid(0, 4), QByteArray::fromHex("01020304"));
    QCOMPARE(out.mid(4, 4), QByteArray::fromHex("FFFFFFFF"));
    QCOMPARE(out.mid(8, 4), QByteArray::fromHex("AABBCCDD"));
}

void TestIntelHex::testSpanOverLimit()
{
    // 总跨度 sanity 上限 16 MiB：两条数据记录相距 17 MiB 以上 → 拒绝解析
    // （防恶意/异常 hex 按覆盖区间展开 OOM）。
    const QByteArray hex =
        ":020000040000FA\r\n" // base=0：sum=02+00+00+04+00+00=0x06 → CC=FA
        ":0400000001020304F2\r\n" // abs=0
        ":020000040110E9\r\n" // base=0x0110（17 MiB）：sum=0x17 → CC=0x100-0x17=E9
        ":0400000001020304F2\r\n" // abs=0x01100000 → 跨度 0x01100004 > 16 MiB
        ":00000001FF\r\n";
    QByteArray out;
    QString err;
    QVERIFY(!IntelHexParser::parse(hex, &out, &err));
    QVERIFY(err.contains(QStringLiteral("跨度")));
}

void TestIntelHex::testBadChecksum()
{
    // 校验和字节 F2 改 F3 → 必须拒绝。
    const QByteArray hex = ":0400000001020304F3\r\n:00000001FF\r\n";
    QByteArray out;
    QString err;
    QVERIFY(!IntelHexParser::parse(hex, &out, &err));
    QVERIFY(err.contains(QStringLiteral("校验和")));
}

void TestIntelHex::testMissingEof()
{
    // 缺少 01 记录 → 拒绝。
    const QByteArray hex = ":0400000001020304F2\r\n";
    QByteArray out;
    QString err;
    QVERIFY(!IntelHexParser::parse(hex, &out, &err));
    QVERIFY(err.contains(QStringLiteral("EOF")));
}

void TestIntelHex::testUnknownRecordType()
{
    // 类型 0x0A（未定义）→ 拒绝。记录布局：count=02, addr=0000, type=0A,
    // data=0000；sum=02+00+00+0A+00+00=0x0C → CC=F4。
    const QByteArray hex = ":0200000A0000F4\r\n:00000001FF\r\n";
    QByteArray out;
    QString err;
    QVERIFY(!IntelHexParser::parse(hex, &out, &err));
    QVERIFY(err.contains(QStringLiteral("记录类型")));
}

int runTestIntelHex(int argc, char** argv)
{
    TestIntelHex test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_intel_hex.moc"