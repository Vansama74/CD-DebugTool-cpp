#pragma once
#include <QByteArray>
#include <QString>
#include <QVector>
#include <QtGlobal>

// IAP frame (32-bit little-endian words on the wire):
//   [0..3]  magic 0x5A5A5A5A
//   [4..7]  seq
//   [8..11] cmd
//   [12..15] payload_len (in words)
//   [16..]  payload words
//   CRC32-MPEG2 over the word stream (each 32-bit word serialized BIG-endian),
//   matching the STM32F4 hardware CRC unit on the device (HAL_CRC_Calculate
//   feeds each word MSB-first) and the Windows reference tool's frames.
class IapFrame {
public:
    static constexpr quint32 FRAME_MAGIC = 0x5A5A5A5Au;
    static constexpr int HEADER_SIZE_BYTES = 16;

    struct ParsedFrame {
        quint32 magic = 0;
        quint32 seq = 0;
        quint32 cmd = 0;
        quint32 payloadLen = 0; // in words
        QVector<quint32> payloadWords;
        bool validCrc = false;
    };

    static QByteArray buildFrame(quint32 cmd, quint32 seq, const QByteArray& payload);
    static QByteArray buildFrameWords(quint32 cmd, quint32 seq,
                                      const QVector<quint32>& words);
    static bool parseFrame(const QByteArray& data, ParsedFrame* out);

    static quint32 packIp(const QString& ip);
    static QString unpackIp(quint32 word);

    // Explicit little-endian word helpers. Hand-rolled to avoid QDataStream's
    // big-endian default, which is the #1 silent-bug source in this protocol.
    static quint32 readLe32(const QByteArray& data, int offset);
    static void appendLe32(QByteArray& out, quint32 v);
};
