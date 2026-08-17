#pragma once
#include <QByteArray>
#include <QVector>
#include <QtGlobal>

// CRC-32/MPEG-2: poly 0x04C11DB7, init 0xFFFFFFFF, no input/output
// reflection, xorout 0x00000000. Table-driven, MSB-first (top-bit test, shift
// left, XOR poly). Distinct from zlib CRC-32 (which reflects I/O).
class Crc32Mpeg2 {
public:
    static quint32 crc32Mpeg2(const QByteArray& data);

    // Serializes each word BIG-endian then computes crc32Mpeg2. Used only for
    // the protocol documentation's self-test vectors (the wire format is LE).
    static quint32 crc32Mpeg2Words(const QVector<quint32>& words);
};
