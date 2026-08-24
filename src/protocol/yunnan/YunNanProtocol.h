#pragma once
#include <QByteArray>
#include <QString>
#include <QtGlobal>

// 云南常规费显协议 (Yunnan highway fee-display protocol, 云南LED费显P5 2022.7.5,
// 版本 YN_FX_P5_1.0)。
//
// Frame envelope (no checksum):
//   0x7B '{' | cmd(1B) | len(1B BINARY) | data[N] | 0x7D '}'
//
// cmd：ASCII '1'~'9','A','B' 与二进制 0x01(全屏点亮)/0x02(获取版本号)。
// len 为二进制字节值（非 ASCII）。串口 9600~115200bps 可调默认 9600，8N1。
// 应答：'1' 主机查询 → 7B 31 01 00 7D（设备恒回正常 00）；
//       0x02 版本号 → 无封套裸 ASCII 文本（设备侧回固件 PROGRAM_CODE，
//       协议文档约定版本号 YN_FX_P5_1.0）；其余命令无应答。
namespace yunnan {

enum class Cmd : char {
    FullScreenLight = 0x01, // 全屏点亮: len==1, DATA0 0x01红~0x07白（设备扩展七色）
    GetVersion      = 0x02, // 获取版本号: len==1 DATA0=0x00; 回裸 ASCII 版本号
    HostQuery       = '1',  // 主机查询: len==0, 回 7B 31 01 00 7D
    SelfCheck       = '2',  // 自检: len==0, 无应答
    OneLine         = '3',  // 单行显示: 颜色'0'~'2' + 行号'1'~'5' + GBK 文本
    FullScreenEdit  = '4',  // 全屏可编辑: 颜色'0'~'2' + X(1B) + Y(1B) + GBK 文本
    ClearAll        = '5',  // 全屏清除: len==0
    ClearLine       = '6',  // 单行清除: 行号'1'~'5'
    CivilVoice      = '7',  // 礼貌语音: '0'~'3'
    Brightness      = '8',  // 亮度: 0x00=自动 / '1'~'8'=手动档（8 最亮）
    Volume          = '9',  // 音量: '1'~'5'
    Peripheral      = 'A',  // 外设: bit0 绿灯 bit1 红灯 bit2 黄闪（二进制位掩码）
    FeeVoice        = 'B',  // 费额语音: 金额 ASCII 串（整数或小数，0 元不播）
};

// Wrap a data payload into a complete frame: 7B cmd len data 7D.
QByteArray buildFrame(Cmd cmd, const QByteArray& payload);

// Payload builders. Each returns the DATA field only (without the envelope).
QByteArray fullScreenLightPayload(int color);            // 1红~7白（设备扩展七色）
QByteArray versionPayload();                             // {0x00}
QByteArray oneLinePayload(int color, int row, const QString& text); // color 0-2, row 1-5, GBK
QByteArray fullScreenEditPayload(int color, quint8 x, quint8 y, const QString& text);
QByteArray clearLinePayload(int row);                    // ASCII '1'~'5'
QByteArray civilVoicePayload(int index);                 // ASCII '0'~'3'
QByteArray brightnessPayload(int level);                 // 0=自动(0x00), 1~8=ASCII '1'~'8'
QByteArray volumePayload(int level);                     // ASCII '1'~'5'
QByteArray peripheralPayload(bool green, bool red, bool yellowFlash); // bit0/bit1/bit2
QByteArray feeVoicePayload(const QString& amount);       // amount ASCII（整数/小数）

struct Frame {
    Cmd cmd = Cmd::HostQuery;
    QByteArray data;
    bool ok = false; // true when the frame shape is valid AND the command is known
};

// Incremental frame parser: feeds arbitrary chunks and extracts complete frames.
class FrameParser {
public:
    void feed(const QByteArray& chunk);
    bool next(Frame* out);        // returns false until a complete frame is available
    bool hasPending() const { return !m_buffer.isEmpty(); } // 仍有悬挂的半帧字节

private:
    QByteArray m_buffer;
};

struct QueryReply {
    bool ok = false;     // parsed a valid HostQuery reply
    bool normal = false; // 0x00 = normal, 0x01 = abnormal
};

// Accepts either the full reply frame (7B 31 01 xx 7D) or the 1-byte data field.
QueryReply parseQueryReply(const QByteArray& frameData);

// 0x02 版本号应答：接受 7B 02 len ... 7D 封套或裸 ASCII 文本，返回解码字符串（GBK）。
QString parseVersionReply(const QByteArray& bytes);

QByteArray toGbk(const QString& s);   // QTextCodec GBK（GB2312 为其子集）
QString fromGbk(const QByteArray& b);

} // namespace yunnan