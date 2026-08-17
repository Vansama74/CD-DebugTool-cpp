#include "Crc32Mpeg2.h"

namespace {

constexpr quint32 POLY = 0x04C11DB7u;
constexpr quint32 INIT = 0xFFFFFFFFu;
constexpr quint32 XOR_OUT = 0x00000000u;

const QVector<quint32>& table()
{
    static const QVector<quint32> t = [] {
        QVector<quint32> tbl(256);
        for (int i = 0; i < 256; ++i) {
            quint32 crc = static_cast<quint32>(i) << 24;
            for (int bit = 0; bit < 8; ++bit) {
                if (crc & 0x80000000u)
                    crc = (crc << 1) ^ POLY;
                else
                    crc = crc << 1;
            }
            tbl[i] = crc;
        }
        return tbl;
    }();
    return t;
}

} // namespace

quint32 Crc32Mpeg2::crc32Mpeg2(const QByteArray& data)
{
    quint32 crc = INIT;
    const QVector<quint32>& t = table();
    for (int i = 0; i < data.size(); ++i) {
        const quint32 byte = static_cast<quint8>(data.at(i));
        const quint32 idx = (crc >> 24) ^ byte;
        crc = (crc << 8) ^ t[idx];
    }
    return (crc ^ XOR_OUT) & 0xFFFFFFFFu;
}

quint32 Crc32Mpeg2::crc32Mpeg2Words(const QVector<quint32>& words)
{
    QByteArray data;
    data.reserve(words.size() * 4);
    for (quint32 w : words) {
        data.append(static_cast<char>((w >> 24) & 0xFFu));
        data.append(static_cast<char>((w >> 16) & 0xFFu));
        data.append(static_cast<char>((w >> 8) & 0xFFu));
        data.append(static_cast<char>(w & 0xFFu));
    }
    return crc32Mpeg2(data);
}
