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

    // Firmware version: 8 words -> 32 bytes ASCII, little-endian, null-trimmed.
    QByteArray versionBytes;
    versionBytes.reserve(32);
    for (int i = 0; i < 8; ++i) {
        const quint32 w = words[2 + i];
        versionBytes.append(static_cast<char>(w & 0xFFu));
        versionBytes.append(static_cast<char>((w >> 8) & 0xFFu));
        versionBytes.append(static_cast<char>((w >> 16) & 0xFFu));
        versionBytes.append(static_cast<char>((w >> 24) & 0xFFu));
    }
    while (!versionBytes.isEmpty() && versionBytes.endsWith('\0'))
        versionBytes.chop(1);
    info.version = QString::fromLatin1(versionBytes);

    info.upgradeState = static_cast<int>(words[10]);
    info.valid = true;
    return info;
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
