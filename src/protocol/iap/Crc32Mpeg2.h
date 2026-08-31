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

    // Serializes each word BIG-endian then computes crc32Mpeg2. This is the
    // IAP wire CRC semantics: the device's STM32F4 hardware CRC unit
    // (HAL_CRC_Calculate) feeds each 32-bit word MSB-first, which is
    // equivalent to CRC-32/MPEG-2 over the per-word big-endian byte stream.
    static quint32 crc32Mpeg2Words(const QVector<quint32>& words);
};
