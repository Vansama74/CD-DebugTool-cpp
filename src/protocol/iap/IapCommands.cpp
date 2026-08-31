#include "IapCommands.h"

#include "IapFrame.h"

QByteArray IapCommands::buildReportIpRequest()
{
    return IapFrame::buildFrame(CMD_REPORT_IP, 0, QByteArray());
}

QByteArray IapCommands::buildSetIpRequest(const QString& ip, const QString& mask,
                                          const QString& gateway, quint16 port)
{
    QVector<quint32> payload;
    payload << IapFrame::packIp(ip) << IapFrame::packIp(mask) << IapFrame::packIp(gateway)
            << static_cast<quint32>(port);
    return IapFrame::buildFrameWords(CMD_SET_IP, 0, payload);
}

QByteArray IapCommands::buildQueryStatusRequest()
{
    return IapFrame::buildFrame(CMD_QUERY_STATUS, 0, QByteArray());
}

QByteArray IapCommands::buildEraseRequest(quint32 firmwareSizeBytes)
{
    const quint32 wordCount = (firmwareSizeBytes + 3) / 4;
    QVector<quint32> payload;
    payload << wordCount;
    return IapFrame::buildFrameWords(CMD_ERASE_FW, 0, payload);
}

QByteArray IapCommands::buildTransferFrame(quint32 seq, const QVector<quint32>& firmwareWords,
                                           int offset, int pageSize)
{
    int end = offset + pageSize;
    if (end > firmwareWords.size())
        end = firmwareWords.size();

    QVector<quint32> chunk;
    if (end > offset)
        chunk = firmwareWords.mid(offset, end - offset);
    return IapFrame::buildFrameWords(CMD_TRANSFER_FW, seq, chunk);
}

QByteArray IapCommands::buildEnterRecoveryRequest()
{
    return IapFrame::buildFrame(CMD_ENTER_RECOVERY, 0, QByteArray());
}

QByteArray IapCommands::buildRebootRequest()
{
    return IapFrame::buildFrame(CMD_REBOOT, 0, QByteArray());
}

ReportIpInfo IapCommands::parseReportIpResponse(const QVector<quint32>& words)
{
    ReportIpInfo info;
    if (words.size() < 4)
        return info;
    info.ip = IapFrame::unpackIp(words[0]);
    info.mask = IapFrame::unpackIp(words[1]);
    info.gateway = IapFrame::unpackIp(words[2]);
    info.appPort = static_cast<quint16>(words[3] & 0xFFFFu);
    info.valid = true;
    return info;
}

StatusInfo IapCommands::parseStatusResponse(const QVector<quint32>& words)
{
    StatusInfo info;
    if (words.size() < 11)
        return info;

    info.fwSize = words[0];
    info.fwCrc = words[1];

    // Firmware version: 8 words -> 32 bytes ASCII。设备端（主固件
    // app_iap_cmd.c cmd_ReportFirmwareStatus_03 与 Recovery cmd.c）每 word
    // 按大端构造（ver[4i]<<24 | ver[4i+1]<<16 | ver[4i+2]<<8 | ver[4i+3]），
    // 与 0x01 的 IP word 构造同构；须按大端拆回字节。旧实现按小端拆导致每组
    // 4 字符反转（"9K1F3127E2" 显示成 "F1K92 7..." 之类），尾部 NUL 裁剪保留。
    QByteArray versionBytes;
    versionBytes.reserve(32);
    for (int i = 0; i < 8; ++i) {
        const quint32 w = words[2 + i];
        versionBytes.append(static_cast<char>((w >> 24) & 0xFFu));
        versionBytes.append(static_cast<char>((w >> 16) & 0xFFu));
        versionBytes.append(static_cast<char>((w >> 8) & 0xFFu));
        versionBytes.append(static_cast<char>(w & 0xFFu));
    }
    while (!versionBytes.isEmpty() && versionBytes.endsWith('\0'))
        versionBytes.chop(1);
    info.version = QString::fromLatin1(versionBytes);

    info.upgradeState = static_cast<int>(words[10]);
    info.valid = true;
    return info;
}

bool IapCommands::parseSetIpResponse(const QVector<quint32>& words)
{
    // 4B02 应答容忍两态：主固件 rtn_cmd02 带 1 word 结果码（0=成功、1=失败，
    // app_iap_cmd.c cmd_ForceModifyIP_02）；Recovery rtn_cmd02 空载荷 ACK
    // （cmd.c cmd_ForceModifyIP_02 直接落盘不应答结果码）。
    if (words.isEmpty())
        return true;
    return words[0] == 0;
}

bool IapCommands::parseEraseResponse(const QVector<quint32>& words)
{
    if (words.isEmpty())
        return false;
    return words[0] == 0;
}

QVector<quint32> IapCommands::parseTransferResponse(const QVector<quint32>& words)
{
    return words;
}
