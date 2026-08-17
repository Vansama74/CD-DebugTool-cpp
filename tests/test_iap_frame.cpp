#include <QtTest>

#include "protocol/iap/IapCommands.h"
#include "protocol/iap/IapFrame.h"

class TestIapFrame : public QObject {
    Q_OBJECT

private slots:
    void testBuildParseRoundTrip();
    void testReportIpRequestBytes();
    void testPackUnpackIp();
};

void TestIapFrame::testBuildParseRoundTrip()
{
    QByteArray payload;
    IapFrame::appendLe32(payload, 0x12345678u);
    IapFrame::appendLe32(payload, 0xDEADBEEFu);

    const QByteArray frame = IapFrame::buildFrame(0x00004B05u, 7u, payload);

    IapFrame::ParsedFrame parsed;
    QVERIFY(IapFrame::parseFrame(frame, &parsed));
    QVERIFY(parsed.validCrc);
    QCOMPARE(parsed.magic, static_cast<quint32>(0x5A5A5A5Au));
    QCOMPARE(parsed.seq, static_cast<quint32>(7u));
    QCOMPARE(parsed.cmd, static_cast<quint32>(0x00004B05u));
    QCOMPARE(parsed.payloadLen, static_cast<quint32>(2u));
    QCOMPARE(parsed.payloadWords.size(), 2);
    QCOMPARE(parsed.payloadWords[0], static_cast<quint32>(0x12345678u));
    QCOMPARE(parsed.payloadWords[1], static_cast<quint32>(0xDEADBEEFu));
}

void TestIapFrame::testReportIpRequestBytes()
{
    // Byte-exact LE wire representation of the report-IP request.
    QCOMPARE(IapCommands::buildReportIpRequest(),
             QByteArray::fromHex("5a5a5a5a00000000014b000000000000f66d1184"));
}

void TestIapFrame::testPackUnpackIp()
{
    QCOMPARE(IapFrame::packIp(QStringLiteral("192.168.114.200")),
             static_cast<quint32>(0xC0A872C8u));
    QCOMPARE(IapFrame::unpackIp(0xC0A872C8u), QStringLiteral("192.168.114.200"));
}

int runTestIapFrame(int argc, char** argv)
{
    TestIapFrame test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_iap_frame.moc"
