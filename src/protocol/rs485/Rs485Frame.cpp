#include "Rs485Frame.h"

quint8 Rs485Frame::xorChecksum(const QByteArray& first4)
{
    quint8 result = 0;
    const int n = qMin(first4.size(), 4);
    for (int i = 0; i < n; ++i)
        result ^= static_cast<quint8>(first4.at(i));
    return result;
}

QByteArray Rs485Frame::buildFrame(quint8 deviceId, quint8 cmd, quint8 data)
{
    QByteArray frame;
    frame.resize(FRAME_SIZE);
    frame[0] = static_cast<char>(HEADER);
    frame[1] = static_cast<char>(deviceId);
    frame[2] = static_cast<char>(cmd);
    frame[3] = static_cast<char>(data);
    frame[4] = static_cast<char>(xorChecksum(frame.left(4)));
    frame[5] = static_cast<char>(TAIL);
    return frame;
}

bool Rs485Frame::parseFrame(const QByteArray& data, quint8* deviceId, quint8* cmd,
                            quint8* dataOut, bool* valid)
{
    if (valid)
        *valid = false;

    if (data.size() != FRAME_SIZE)
        return false;

    const quint8 header = static_cast<quint8>(data.at(0));
    const quint8 dev = static_cast<quint8>(data.at(1));
    const quint8 c = static_cast<quint8>(data.at(2));
    const quint8 d = static_cast<quint8>(data.at(3));
    const quint8 checksum = static_cast<quint8>(data.at(4));
    const quint8 tail = static_cast<quint8>(data.at(5));

    if (header != HEADER || tail != TAIL)
        return false;

    if (deviceId)
        *deviceId = dev;
    if (cmd)
        *cmd = c;
    if (dataOut)
        *dataOut = d;

    if (valid)
        *valid = (checksum == xorChecksum(data.left(4)));
    return true;
}

QString Rs485Frame::frameToHex(const QByteArray& frame)
{
    QString result;
    result.reserve(frame.size() * 3);
    for (int i = 0; i < frame.size(); ++i) {
        if (i > 0)
            result += QLatin1Char(' ');
        result += QStringLiteral("%1")
                      .arg(static_cast<int>(static_cast<quint8>(frame.at(i))), 2, 16,
                           QLatin1Char('0'))
                      .toUpper();
    }
    return result;
}
