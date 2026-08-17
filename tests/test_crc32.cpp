#include <QtTest>

#include "protocol/iap/Crc32Mpeg2.h"
#include "protocol/iap/IapCommands.h"
#include "protocol/iap/IapFrame.h"

class TestCrc32 : public QObject {
    Q_OBJECT

private slots:
    void testReportIpRequestLeWireCrc();
    void testDocBeWordVectors();
};

void TestCrc32::testReportIpRequestLeWireCrc()
{
    // LE-wire report-IP request frame: the trailing 4 bytes are the CRC-32/MPEG-2
    // over header+payload, value 0x84116DF6, serialized little-endian.
    const QByteArray frame = IapCommands::buildReportIpRequest();
    QVERIFY(frame.size() >= 4);
    QCOMPARE(frame.right(4), QByteArray::fromHex("f66d1184"));
    QCOMPARE(IapFrame::readLe32(frame, frame.size() - 4),
             static_cast<quint32>(0x84116DF6u));
}

void TestCrc32::testDocBeWordVectors()
{
    // Protocol documentation self-test vectors (BIG-endian word serialization,
    // crc32Mpeg2Words). These exercise the CRC against the doc's own numbers;
    // they differ from the LE wire CRC above because the words are serialized
    // big-endian instead of little-endian.

    // Test 1: report-IP request logical words -> 0x84488377.
    QCOMPARE(Crc32Mpeg2::crc32Mpeg2Words(
                 {0x5A5A5A5Au, 0u, 0x00004B01u, 0u}),
             static_cast<quint32>(0x84488377u));

    // Test 2: report-IP response (8 words) -> 0x0D3BD79F.
    QCOMPARE(Crc32Mpeg2::crc32Mpeg2Words(
                 {0x5A5A5A5Au, 0u, 0x0000B401u, 0x00000004u,
                  0xC0A872C8u, 0xFFFFFF00u, 0xC0A87201u, 0x00002538u}),
             static_cast<quint32>(0x0D3BD79Fu));

    // Test 3: enter-recovery-mode request -> 0x7FABA863.
    QCOMPARE(Crc32Mpeg2::crc32Mpeg2Words(
                 {0x5A5A5A5Au, 0u, 0x00004B06u, 0u}),
             static_cast<quint32>(0x7FABA863u));
}

int runTestCrc32(int argc, char** argv)
{
    TestCrc32 test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_crc32.moc"
