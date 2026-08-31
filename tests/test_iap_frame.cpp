#include <QtTest>

#include "protocol/iap/IapCommands.h"
#include "protocol/iap/IapFrame.h"

class TestIapFrame : public QObject {
    Q_OBJECT

private slots:
    void testBuildParseRoundTrip();
    void testReportIpRequestBytes();
    void testPackUnpackIp();
    void testReportIpResponsePortSemantics();
    void testQueryStatusRequestBytes();
    void testQueryStatusResponseParsing();
    void testSetIpRequestBytes();
    void testSetIpResponseTwoStates();
    void testEraseRequestBytes();
    void testTransferFrameBytes();
    void testRetransmitFrameConstruction();
    void testEnterRecoveryAndRebootRequestBytes();
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
    // CRC 0x84488377（LE 序列化 77 83 48 84）= CRC32(MPEG-2) 按字流计算，
    // 对齐设备端 STM32F4 硬件 CRC 与 Windows 参考工具硬编码帧字节。
    QCOMPARE(IapCommands::buildReportIpRequest(),
             QByteArray::fromHex("5a5a5a5a00000000014b00000000000077834884"));
}

void TestIapFrame::testPackUnpackIp()
{
    QCOMPARE(IapFrame::packIp(QStringLiteral("192.168.114.200")),
             static_cast<quint32>(0xC0A872C8u));
    QCOMPARE(IapFrame::unpackIp(0xC0A872C8u), QStringLiteral("192.168.114.200"));
}

void TestIapFrame::testReportIpResponsePortSemantics()
{
    // 单播端口 bug 修复（2026-08-24）：4B01 应答第 4 word 上报的是 TCP 业务口
    // （9528），须原样保留供 UI 显示/第三方工具连接参考；IAP 单播目标端口恒为
    // IAP_PORT(10011)，与上报端口解耦。
    const QVector<quint32> words = {
        IapFrame::packIp(QStringLiteral("192.168.114.200")), // ip
        IapFrame::packIp(QStringLiteral("255.255.255.0")),   // mask
        IapFrame::packIp(QStringLiteral("192.168.114.1")),   // gateway
        9528u,                                               // TCP 业务口（非 IAP 口）
    };
    const ReportIpInfo info = IapCommands::parseReportIpResponse(words);
    QVERIFY(info.valid);
    QCOMPARE(info.ip, QStringLiteral("192.168.114.200"));
    QCOMPARE(info.appPort, static_cast<quint16>(9528)); // 上报值原样保留（UI 显示语义）

    // 单播目标端口常量恒为 IAP 口 10011；回归守卫：不得被改回 appPort 语义。
    QCOMPARE(IapCommands::IAP_PORT, static_cast<quint16>(10011));
    QVERIFY(IapCommands::IAP_PORT != info.appPort);
}

void TestIapFrame::testQueryStatusRequestBytes()
{
    // 4B03 查询状态请求：无载荷。CRC 按字流 MPEG-2 = 0x16524C6D，
    // 小端序列化 6D 4C 52 16（独立 Python 实现计算）。
    QCOMPARE(IapCommands::buildQueryStatusRequest(),
             QByteArray::fromHex("5a5a5a5a00000000034b0000000000006d4c5216"));
}

void TestIapFrame::testQueryStatusResponseParsing()
{
    // 4B03 应答解析（大端版本 word 修复）：设备端（主固件 app_iap_cmd.c
    // cmd_ReportFirmwareStatus_03 与 Recovery cmd.c）version 每 word 大端
    // 构造（ver[4i]<<24 | ver[4i+1]<<16 | ver[4i+2]<<8 | ver[4i+3]），与 0x01
    // 的 IP word 同构。旧实现按小端拆字节导致每组 4 字符反转
    // （"9K1F3127E2" → "F1K91372 2E" 之类）。硬编码 "9K1F3127E2"：
    // word0=0x394B3146, word1=0x33313237, word2=0x45320000，其余 0；
    // 尾部 NUL 裁剪后应精确等于 PROGRAM_CODE（10 字符）。
    const QVector<quint32> words = {
        234404u,     // fwSize（字节）
        0x65AB7EDAu, // fwCrc
        0x394B3146u, 0x33313237u, 0x45320000u, 0u, 0u, 0u, 0u, 0u, // version 8 words
        0u,          // update_sta（APP_BOARD_UPDATED）
    };
    const StatusInfo info = IapCommands::parseStatusResponse(words);
    QVERIFY(info.valid);
    QCOMPARE(info.fwSize, 234404u);
    QCOMPARE(info.fwCrc, 0x65AB7EDAu);
    QCOMPARE(info.version, QStringLiteral("9K1F3127E2"));
    QCOMPARE(info.upgradeState, 0);
}

void TestIapFrame::testSetIpRequestBytes()
{
    // 4B02 setip 下发帧：载荷 4 word = ip/mask/gw/port。三个 IP word 大端
    // 语义（packIp：192.168.114.200 → 0xC0A872C8），port word 直接端口值
    // （9528 = 0x2538）。线上小端序列化。CRC = 0x147517E2 → 小端 e2 17 75
    // 14（MSB-first MPEG-2 字流，独立 Python 实现计算）。
    QCOMPARE(IapCommands::buildSetIpRequest(QStringLiteral("192.168.114.200"),
                                            QStringLiteral("255.255.255.0"),
                                            QStringLiteral("192.168.114.1"), 9528),
             QByteArray::fromHex("5a5a5a5a00000000024b000004000000"
                                 "c872a8c000ffffff0172a8c038250000"
                                 "e2177514"));
}

void TestIapFrame::testSetIpResponseTwoStates()
{
    // 4B02 应答两态：主固件 rtn_cmd02 带 1 word 结果码（0=成功、非 0=失败）；
    // Recovery rtn_cmd02 空载荷 ACK。两态均须被正确判定。
    QVERIFY(IapCommands::parseSetIpResponse({}));
    QVERIFY(IapCommands::parseSetIpResponse({0u}));
    QVERIFY(!IapCommands::parseSetIpResponse({1u}));
}

void TestIapFrame::testEraseRequestBytes()
{
    // 4B04 擦除请求：载荷 1 word = 固件 word 数（4096 bytes → 1024 words =
    // 0x400）。CRC = 0x97019D4A → 小端 4A 9D 01 97。设备 Recovery
    // cmd_PrepareUpgrade_04 按该 word 数计算擦除扇区数。
    QCOMPARE(IapCommands::buildEraseRequest(4096),
             QByteArray::fromHex("5a5a5a5a00000000044b000001000000000400004a9d0197"));
}

void TestIapFrame::testTransferFrameBytes()
{
    // 4B05 传输帧 seq=1，载荷 2 word {0x11223344, 0x55667788}（小端线上）。
    // CRC = 0xEAA0BB6E → 小端 6E BB A0 EA。
    QCOMPARE(IapCommands::buildTransferFrame(1, {0x11223344u, 0x55667788u}, 0),
             QByteArray::fromHex("5a5a5a5a01000000054b00000200000044332211887766556ebba0ea"));
}

void TestIapFrame::testRetransmitFrameConstruction()
{
    // 缺失帧重传构造：rtn_cmd05 缺失列表为 0-based 帧索引，重传帧 seq = idx+1。
    // 载荷 = 固件字数组中该页的原始字（小端读出），对齐设备 Recovery
    // cmd_SendUpgradePackage_05 的写 flash 地址偏移 frame_seq * 1024 字节
    // （= idx * 256 word）。2000 words → 8 页（ceil(2000/256)），idx 2/5 均为整页。
    QVector<quint32> words;
    for (quint32 i = 0; i < 2000; ++i)
        words.append(0xDEAD0000u + i);
    const int page = IapCommands::PAGE_SIZE_WORDS;

    const quint32 missIdx[] = {2u, 5u};
    for (quint32 idx : missIdx) {
        const quint32 seq = idx + 1;
        const QByteArray frame =
            IapCommands::buildTransferFrame(seq, words, static_cast<int>(idx) * page);
        IapFrame::ParsedFrame p;
        QVERIFY(IapFrame::parseFrame(frame, &p));
        QVERIFY(p.validCrc);
        QCOMPARE(p.cmd, IapCommands::CMD_TRANSFER_FW);
        QCOMPARE(p.seq, seq);
        QCOMPARE(p.payloadLen, static_cast<quint32>(page));
        for (int i = 0; i < page; ++i)
            QCOMPARE(p.payloadWords[i], words[static_cast<int>(idx) * page + i]);
    }

    // 末页裁剪：2000 words → 第 8 页（seq=8, idx=7）只含 2000-7*256=208 words。
    const QByteArray last = IapCommands::buildTransferFrame(8, words, 7 * page);
    IapFrame::ParsedFrame p;
    QVERIFY(IapFrame::parseFrame(last, &p));
    QVERIFY(p.validCrc);
    QCOMPARE(p.payloadLen, 208u);
    for (int i = 0; i < 208; ++i)
        QCOMPARE(p.payloadWords[i], words[7 * page + i]);
}

void TestIapFrame::testEnterRecoveryAndRebootRequestBytes()
{
    // 4B06 进入升级模式请求：无载荷。CRC = 0x7FABA863 → 小端 63 A8 AB 7F
    // （与 test_crc32 的文档向量一致）。主固件收到后置 RTC backup
    // FLAG_FORCE_UPDATE 并 ACK len=0，不重启。
    QCOMPARE(IapCommands::buildEnterRecoveryRequest(),
             QByteArray::fromHex("5a5a5a5a00000000064b00000000000063a8ab7f"));
    // 4B07 重启请求：无载荷。CRC = 0x36A6CFEE → 小端 EE CF A6 36。
    QCOMPARE(IapCommands::buildRebootRequest(),
             QByteArray::fromHex("5a5a5a5a00000000074b000000000000eecfa636"));
}

int runTestIapFrame(int argc, char** argv)
{
    TestIapFrame test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_iap_frame.moc"
