#include "IapFrame.h"

#include "Crc32Mpeg2.h"

#include <QStringList>

quint32 IapFrame::readLe32(const QByteArray& data, int offset)
{
    return (static_cast<quint32>(static_cast<quint8>(data.at(offset))))
        | (static_cast<quint32>(static_cast<quint8>(data.at(offset + 1))) << 8)
        | (static_cast<quint32>(static_cast<quint8>(data.at(offset + 2))) << 16)
        | (static_cast<quint32>(static_cast<quint8>(data.at(offset + 3))) << 24);
}

void IapFrame::appendLe32(QByteArray& out, quint32 v)
{
    out.append(static_cast<char>(v & 0xFFu));
    out.append(static_cast<char>((v >> 8) & 0xFFu));
    out.append(static_cast<char>((v >> 16) & 0xFFu));
    out.append(static_cast<char>((v >> 24) & 0xFFu));
}

QByteArray IapFrame::buildFrame(quint32 cmd, quint32 seq, const QByteArray& payload)
{
    Q_ASSERT((payload.size() % 4) == 0);
    const quint32 payloadLenWords = static_cast<quint32>(payload.size() / 4);

    QByteArray header;
    header.reserve(HEADER_SIZE_BYTES);
    appendLe32(header, FRAME_MAGIC);
    appendLe32(header, seq);
    appendLe32(header, cmd);
    appendLe32(header, payloadLenWords);

    const QByteArray crcData = header + payload;
    const quint32 crc = Crc32Mpeg2::crc32Mpeg2(crcData);

    QByteArray out = crcData;
    appendLe32(out, crc);
    return out;
}

QByteArray IapFrame::buildFrameWords(quint32 cmd, quint32 seq, const QVector<quint32>& words)
{
    QByteArray payload;
    payload.reserve(words.size() * 4);
    for (quint32 w : words)
        appendLe32(payload, w);
    return buildFrame(cmd, seq, payload);
}

bool IapFrame::parseFrame(const QByteArray& data, ParsedFrame* out)
{
    const int minSize = HEADER_SIZE_BYTES + 4; // header + CRC
    if (data.size() < minSize)
        return false;

    const quint32 magic = readLe32(data, 0);
    const quint32 seq = readLe32(data, 4);
    const quint32 cmd = readLe32(data, 8);
    const quint32 payloadLen = readLe32(data, 12);

    if (magic != FRAME_MAGIC)
        return false;

    const int expected = HEADER_SIZE_BYTES + static_cast<int>(payloadLen) * 4 + 4;
    if (data.size() < expected)
        return false;

    QVector<quint32> payloadWords;
    payloadWords.reserve(static_cast<int>(payloadLen));
    for (quint32 i = 0; i < payloadLen; ++i)
        payloadWords.append(readLe32(data, HEADER_SIZE_BYTES + static_cast<int>(i) * 4));

    const int crcOffset = HEADER_SIZE_BYTES + static_cast<int>(payloadLen) * 4;
    const quint32 crcRecv = readLe32(data, crcOffset);
    const quint32 crcCalc = Crc32Mpeg2::crc32Mpeg2(data.left(crcOffset));

    if (out) {
        out->magic = magic;
        out->seq = seq;
        out->cmd = cmd;
        out->payloadLen = payloadLen;
        out->payloadWords = payloadWords;
        out->validCrc = (crcRecv == crcCalc);
    }
    return true;
}

quint32 IapFrame::packIp(const QString& ip)
{
    const QStringList parts = ip.split(QLatin1Char('.'));
    if (parts.size() != 4)
        return 0;
    return (parts[0].toUInt() << 24) | (parts[1].toUInt() << 16)
        | (parts[2].toUInt() << 8) | parts[3].toUInt();
}

QString IapFrame::unpackIp(quint32 word)
{
    return QStringLiteral("%1.%2.%3.%4")
        .arg((word >> 24) & 0xFFu)
        .arg((word >> 16) & 0xFFu)
        .arg((word >> 8) & 0xFFu)
        .arg(word & 0xFFu);
}
