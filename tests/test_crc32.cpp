#include <QtTest>

#include "protocol/iap/Crc32Mpeg2.h"
#include "protocol/iap/IapCommands.h"
#include "protocol/iap/IapFrame.h"

class TestCrc32 : public QObject {
    Q_OBJECT

private slots:
    void testReportIpRequestLeWireCrc();
    void testDocBeWordVectors();
    void testFirmwareCrcPadWords();
};

void TestCrc32::testReportIpRequestLeWireCrc()
{
    // LE-wire report-IP request frame: the trailing 4 bytes are the CRC-32/MPEG-2
    // over the word stream (per-word big-endian), value 0x84488377, serialized
    // little-endian. 对齐设备端 STM32F4 硬件 CRC（HAL_CRC_Calculate 逐 word
    // MSB-first）与 Windows 参考工具硬编码帧字节 77 83 48 84。
    const QByteArray frame = IapCommands::buildReportIpRequest();
    QVERIFY(frame.size() >= 4);
    QCOMPARE(frame.right(4), QByteArray::fromHex("77834884"));
    QCOMPARE(IapFrame::readLe32(frame, frame.size() - 4),
             static_cast<quint32>(0x84488377u));
}

void TestCrc32::testDocBeWordVectors()
{
    // 协议/设备端 CRC 自测向量（BIG-endian 字流序列化，crc32Mpeg2Words）。
    // 线上 CRC 即按此字流计算；对无载荷帧，字流与线上字节的唯一区别是
    // cmd 字在线上为小端（如 01 4B 00 00），CRC 输入为大端（00 00 4B 01）。

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

void TestCrc32::testFirmwareCrcPadWords()
{
    // 固件 CRC 语义（UpgradeEngine::loadFirmware / UpgradeWorker）：
    // 0xFF 填充到 4B 对齐 + crc32Mpeg2Words（逐 word 大端 MPEG-2）。
    // 5 字节固件 {01 02 03 04 05} → 填充 {05 FF FF FF} → words
    // {0x01020304, 0x05FFFFFF}，期望 0x497B8808（独立 Python 实现算出），
    // 与设备 Recovery 写 flash 后 HAL_CRC_Calculate（word 流大端）及 Java
    // CRC32_OR_MPEG_2(int[]) 一致。注意线上/内存中的 word 值按小端读出
    // （readLe32：文件字节 {01 02 03 04 | 05 FF FF FF} → words {0x04030201,
    // 0xFFFFFF05}）。旧实现（逐字节 crc32Mpeg2 且不填充）结果为 0xE28F4B83，
    // 与设备 CRC 恒不等。
    QCOMPARE(Crc32Mpeg2::crc32Mpeg2Words({0x04030201u, 0xFFFFFF05u}),
             static_cast<quint32>(0xCCD0E62Cu));
    // 对齐 4B 时填充为空：4096 字节 0xAB → 1024 word 0xABABABAB。
    QVector<quint32> words(1024, 0xABABABABu);
    QCOMPARE(Crc32Mpeg2::crc32Mpeg2Words(words), static_cast<quint32>(0xAA61A1F3u));
}

int runTestCrc32(int argc, char** argv)
{
    TestCrc32 test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_crc32.moc"
