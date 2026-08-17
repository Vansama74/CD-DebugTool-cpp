#pragma once
#include <QByteArray>
#include <QString>
#include <QtGlobal>

// 青海高速费显协议 (QingHai highway fee-display protocol).
//
// Frame envelope (no checksum):
//   0x7B | cmd(1 ASCII byte) | len(1 BINARY byte) | data[N] | 0x7D
//
// The length byte is BINARY (0-255), not ASCII digits. Only command '1'
// (HostQuery) produces a reply: 7B 31 01 00 7D (0x00 = normal; 0x01 = abnormal,
// which the firmware never emits in practice).
namespace qinghai {

enum class Cmd : char {
    HostQuery    = '1', // 查询: empty payload, replies 7B 31 01 00 7D
    SelfCheck    = '2', // 自检: empty payload, no reply
    OneLine      = '3', // 单行显示: color ASCII + row ASCII + GBK text
    FullScreen   = '4', // 全屏显示: color ASCII + x/y BINARY + GBK text
    Clear        = '5', // 清屏: empty payload
    FixedDisplay = '6', // 固定显示: type ASCII + '|'-separated GBK fields
    CivilVoice   = '7', // 文明语音: index ASCII '0'..'3'
    Brightness   = '8', // 亮度: level ASCII '0'(auto)..'5'
    Volume       = '9', // 音量: level ASCII '1'..'5'
    Peripheral   = 'A', // 外设: BINARY bitmask (green=1, red=2, yellow=4)
    Voice        = 'B', // 费额语音: type ASCII + 5 ASCII digits (amount in fen)
};

// Wrap a data payload into a complete frame: 7B cmd len data 7D.
QByteArray buildFrame(Cmd cmd, const QByteArray& payload);

// Payload builders. Each returns the DATA field only (without the envelope).
QByteArray oneLinePayload(int color, int row, const QString& text);      // color 0-2, row 1-5, GBK
QByteArray fullScreenPayload(int color, quint8 x, quint8 y, const QString& text); // x/y binary
QByteArray fixedDisplayPayload(int type, const QString& rawPipeSeparated); // type 0(客车)/1(货车)
QByteArray indexPayload(int index);     // ASCII '0'..'9' (used by '7' CivilVoice)
QByteArray levelPayload(int level);     // ASCII digit ('0'..'5' etc.)
QByteArray peripheralPayload(bool green, bool red, bool yellow); // binary bitmask
QByteArray feeVoicePayload(int type, const QString& amountFen);  // type byte + 5 zero-padded digits

struct Frame {
    Cmd cmd = Cmd::HostQuery;
    QByteArray data;
    bool ok = false; // true when the frame shape is valid AND the command is known
};

// Incremental frame parser: feeds arbitrary chunks and extracts complete frames.
class FrameParser {
public:
    void feed(const QByteArray& chunk);
    bool next(Frame* out); // returns false until a complete frame is available

private:
    QByteArray m_buffer;
};

struct QueryReply {
    bool ok = false;     // parsed a valid HostQuery reply
    bool normal = false; // 0x00 = normal, 0x01 = abnormal
};

// Accepts either the full reply frame (7B 31 01 xx 7D) or the 1-byte data field.
QueryReply parseQueryReply(const QByteArray& frameData);

QByteArray toGbk(const QString& s);   // QTextCodec GBK, fallback to locale/UTF-8
QString fromGbk(const QByteArray& b);

} // namespace qinghai
