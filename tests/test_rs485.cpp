#include <QtTest>

#include "protocol/rs485/Rs485Commands.h"
#include "protocol/rs485/Rs485Frame.h"

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

class TestRs485 : public QObject {
    Q_OBJECT

private slots:
    void testXorChecksum();
    void testBuildDisplayStateFrame();
    void testBuildQueryDisplayStateFrame();
    void testBuildBrightnessFrame();
    void testBuildQueryBrightnessFrame();
    void testBuildDeviceIdFrame();
    void testBuildBaudRateFrame();
    void testBuildDacScaleFrames();
    void testCombineDisplay();
    void testParseFrame();
    void testFrameToHex();
    void testDescribeDisplayState();
    void testDescribeBrightness();
};

void TestRs485::testXorChecksum()
{
    QCOMPARE(static_cast<int>(Rs485Frame::xorChecksum(bytes({0xCC, 0x01, 0x01, 0x21}))), 0xED);
    QCOMPARE(static_cast<int>(Rs485Frame::xorChecksum(bytes({0xCC, 0x01, 0x05, 0x02}))), 0xCA);
    QCOMPARE(static_cast<int>(Rs485Frame::xorChecksum(bytes({0xCC, 0x01, 0x08, 0x01}))), 0xC4);
    // Extra trailing bytes are ignored (only the first 4 are XORed).
    QCOMPARE(static_cast<int>(Rs485Frame::xorChecksum(bytes({0xCC, 0x01, 0x05, 0x02, 0xFF}))), 0xCA);
}

void TestRs485::testBuildDisplayStateFrame()
{
    // Python self-test (authoritative): GREEN front (0x20) + RED back (0x01).
    QCOMPARE(Rs485Commands::buildDisplayStateFrame(1, Rs485Commands::FRONT_GREEN,
                                                   Rs485Commands::BACK_RED),
             bytes({0xCC, 0x01, 0x01, 0x21, 0xED, 0xDD}));
}

void TestRs485::testBuildQueryDisplayStateFrame()
{
    // Python self-test (authoritative).
    QCOMPARE(Rs485Commands::buildQueryDisplayStateFrame(1),
             bytes({0xCC, 0x01, 0x02, 0x00, 0xCF, 0xDD}));
}

void TestRs485::testBuildBrightnessFrame()
{
    // Python self-test (authoritative): 80 == 0x50.
    QCOMPARE(Rs485Commands::buildBrightnessFrame(1, 80),
             bytes({0xCC, 0x01, 0x03, 0x50, 0x9E, 0xDD}));
}

void TestRs485::testBuildQueryBrightnessFrame()
{
    // Checksum 0xCC^0x01^0x04^0x00 = 0xC9 (verified via xorChecksum below).
    QCOMPARE(static_cast<int>(Rs485Frame::xorChecksum(bytes({0xCC, 0x01, 0x04, 0x00}))), 0xC9);
    QCOMPARE(Rs485Commands::buildQueryBrightnessFrame(1),
             bytes({0xCC, 0x01, 0x04, 0x00, 0xC9, 0xDD}));
}

void TestRs485::testBuildDeviceIdFrame()
{
    // The task brief wrote "C9" here, but XOR(0xCC,0x01,0x05,0x02)=0xCA, which
    // matches the Python self-test `build_device_id_frame(1,2)` == ...CA DD.
    QCOMPARE(static_cast<int>(Rs485Frame::xorChecksum(bytes({0xCC, 0x01, 0x05, 0x02}))), 0xCA);
    QCOMPARE(Rs485Commands::buildDeviceIdFrame(1, 2),
             bytes({0xCC, 0x01, 0x05, 0x02, 0xCA, 0xDD}));
}

void TestRs485::testBuildBaudRateFrame()
{
    // The task brief wrote "C3" here, but XOR(0xCC,0x01,0x08,0x01)=0xC4, which
    // matches the Python self-test `build_baud_rate_frame(1,1)` == ...C4 DD.
    QCOMPARE(static_cast<int>(Rs485Frame::xorChecksum(bytes({0xCC, 0x01, 0x08, 0x01}))), 0xC4);
    QCOMPARE(Rs485Commands::buildBaudRateFrame(1, 1),
             bytes({0xCC, 0x01, 0x08, 0x01, 0xC4, 0xDD}));
}

void TestRs485::testBuildDacScaleFrames()
{
    // Python self-test (authoritative).
    QCOMPARE(Rs485Commands::buildDacScaleRedFrame(1, 20),
             bytes({0xCC, 0x01, 0x09, 0x14, 0xD0, 0xDD}));
    QCOMPARE(Rs485Commands::buildDacScaleGreenFrame(1, 20),
             bytes({0xCC, 0x01, 0x0A, 0x14, 0xD3, 0xDD}));
    QCOMPARE(Rs485Commands::buildDacScaleRedFrame(1, 16),
             bytes({0xCC, 0x01, 0x09, 0x10, 0xD4, 0xDD}));

    // Clamp to [1, 40].
    QByteArray f = Rs485Commands::buildDacScaleRedFrame(1, 200);
    QCOMPARE(static_cast<int>(static_cast<quint8>(f.at(3))), 40);
    f = Rs485Commands::buildDacScaleRedFrame(1, 0);
    QCOMPARE(static_cast<int>(static_cast<quint8>(f.at(3))), 1);
    f = Rs485Commands::buildDacScaleGreenFrame(1, 255);
    QCOMPARE(static_cast<int>(static_cast<quint8>(f.at(3))), 40);
}

void TestRs485::testCombineDisplay()
{
    QCOMPARE(static_cast<int>(Rs485Commands::combineDisplay(0x20, 0x01)), 0x21);
    QCOMPARE(static_cast<int>(Rs485Commands::combineDisplay(0x30, 0x03)), 0x33);
    QCOMPARE(static_cast<int>(Rs485Commands::combineDisplay(0x00, 0x00)), 0x00);
}

void TestRs485::testParseFrame()
{
    quint8 deviceId = 0, cmd = 0, dataOut = 0;
    bool valid = false;

    const QByteArray frame = Rs485Commands::buildDisplayStateFrame(1, 0x20, 0x01);
    QVERIFY(Rs485Frame::parseFrame(frame, &deviceId, &cmd, &dataOut, &valid));
    QVERIFY(valid);
    QCOMPARE(static_cast<int>(deviceId), 1);
    QCOMPARE(static_cast<int>(cmd), 0x01);
    QCOMPARE(static_cast<int>(dataOut), 0x21);

    // Corrupted checksum: shape still parses, but valid == false.
    QByteArray bad = frame;
    bad[4] = static_cast<char>(bad.at(4) ^ 0xFF);
    valid = true;
    QVERIFY(Rs485Frame::parseFrame(bad, &deviceId, &cmd, &dataOut, &valid));
    QVERIFY(!valid);

    // Bad header / tail / length return false.
    QByteArray badHeader = frame;
    badHeader[0] = 0xAA;
    QVERIFY(!Rs485Frame::parseFrame(badHeader, nullptr, nullptr, nullptr, nullptr));

    QByteArray badTail = frame;
    badTail[5] = 0xAA;
    QVERIFY(!Rs485Frame::parseFrame(badTail, nullptr, nullptr, nullptr, nullptr));

    QVERIFY(!Rs485Frame::parseFrame(frame.left(5), nullptr, nullptr, nullptr, nullptr));
    QVERIFY(!Rs485Frame::parseFrame(frame + QByteArray(1, '\0'), nullptr, nullptr,
                                    nullptr, nullptr));
}

void TestRs485::testFrameToHex()
{
    QCOMPARE(Rs485Frame::frameToHex(bytes({0xCC, 0x01, 0x01, 0x21, 0xED, 0xDD})),
             QStringLiteral("CC 01 01 21 ED DD"));
    QCOMPARE(Rs485Frame::frameToHex(bytes({0x00, 0x0A, 0xFF})),
             QStringLiteral("00 0A FF"));
}

void TestRs485::testDescribeDisplayState()
{
    QCOMPARE(Rs485Commands::describeDisplayState(0x21), QStringLiteral("正面:绿 背面:红"));
    QCOMPARE(Rs485Commands::describeDisplayState(0x00), QStringLiteral("正面:关闭 背面:关闭"));
    QCOMPARE(Rs485Commands::describeDisplayState(0x33), QStringLiteral("正面:转 背面:转"));
}

void TestRs485::testDescribeBrightness()
{
    QCOMPARE(Rs485Commands::describeBrightness(0xFF), QStringLiteral("自动调光"));
    QCOMPARE(Rs485Commands::describeBrightness(80), QStringLiteral("80%"));
    QCOMPARE(Rs485Commands::describeBrightness(0), QStringLiteral("0%"));
}

int runTestRs485(int argc, char** argv)
{
    TestRs485 test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_rs485.moc"
