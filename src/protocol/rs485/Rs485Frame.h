#pragma once
#include <QByteArray>
#include <QString>
#include <QtGlobal>

// RS485 lane-indicator frame: fixed 6 bytes
//   [0xCC][device_id][cmd][data][XOR(prev 4)][0xDD]
class Rs485Frame {
public:
    static constexpr quint8 HEADER = 0xCC;
    static constexpr quint8 TAIL = 0xDD;
    static constexpr int FRAME_SIZE = 6;

    // XOR of the first 4 bytes (header, device_id, cmd, data).
    static quint8 xorChecksum(const QByteArray& first4);

    // Build a 6-byte frame with the checksum computed over the first 4 bytes.
    static QByteArray buildFrame(quint8 deviceId, quint8 cmd, quint8 data);

    // Parse a 6-byte frame. Returns false when the frame is not a valid
    // RS485 frame shape (wrong length, header, or tail). When the shape is
    // valid, the out-params are filled and *valid reports the checksum result.
    // All out-params may be nullptr.
    static bool parseFrame(const QByteArray& data, quint8* deviceId, quint8* cmd,
                           quint8* dataOut, bool* valid);

    // Uppercase, space-separated hex dump ("CC 01 01 21 ED DD").
    static QString frameToHex(const QByteArray& frame);
};
