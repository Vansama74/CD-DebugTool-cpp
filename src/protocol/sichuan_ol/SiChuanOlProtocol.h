#pragma once
#include <QByteArray>
#include <QString>
#include <QtGlobal>

// 四川治超屏费显协议（1F，3.5.1 串口方式）——上位机发送侧与设备应答解析。
//
// 帧结构：FF + 长度(1B，含头尾总长 07~1E) + 命令 + 亮度(00~FF) + 数据 + BCC + FF。
// BCC = 帧头/长度/命令/亮度/数据段五字段逐字节异或（不含尾部 FF）。
//   80 全屏显示（数据段可变长 ≤24B）；81~88 第 1~8 行显示（16B/行，帧长 22）；
//   94 清屏；96 亮度（00=自动调光）；99 通行灯（00红/01绿）；98 黄闪（00关/01开）。
//   以上控制/查询类短帧恒为 7 字节（数据段 1 字节，清屏/查询为 00）；
//   查询类（设备应答）：A0 取显示内容 → A1~A8 每行独立帧；
//   B6 取亮度 / B9 取通行灯 / B8 取黄闪 → 各回一帧（数据段 1B 当前值）。
// 波特率 9600。
namespace sc_ol {

constexpr quint8 kHead       = 0xFF;
constexpr quint8 kTail       = 0xFF;
constexpr quint8 kBrightMax  = 0xFF; // 显示帧亮度字段默认最亮
constexpr int kFrameLenMin   = 7;
constexpr int kFrameLenMax   = 0x1E; // 长度字段上限 30
constexpr int kBytesPerLine  = 16;   // 8 列 × 2
constexpr int kLineCount     = 8;    // 与固件一致：8 行
constexpr int kFullMaxBytes  = kFrameLenMax - 6; // 全屏数据段 ≤24B

enum class Cmd : quint8 {
    FullScreen   = 0x80,
    Line1        = 0x81,
    Line2        = 0x82,
    Line3        = 0x83,
    Line4        = 0x84,
    Line5        = 0x85,
    Line6        = 0x86,
    Line7        = 0x87,
    Line8        = 0x88,
    Clear        = 0x94,
    Brightness   = 0x96,
    LaneLight    = 0x99,
    YellowFlash  = 0x98,
    QueryContent = 0xA0,
    QueryBright  = 0xB6,
    QueryLane    = 0xB9,
    QueryFlash   = 0xB8,
};

// BCC：帧头(含)到数据段(含)逐字节异或，即帧的 [0, size-3] 区间。
quint8 bcc(const QByteArray& raw);

// 拼帧：FF + len + cmd + bright + data + BCC + FF。
// 显示类命令 bright 传 kBrightMax，控制/查询类按协议传 0x00。
QByteArray buildFrame(quint8 cmd, quint8 bright, const QByteArray& data);

QByteArray lineFrame(int row, const QString& text); // 81~88，16B 数据区
QByteArray fullScreenFrame(const QString& text);    // 80，可变长 ≤24B
QByteArray clearFrame();                            // 94
QByteArray brightnessFrame(quint8 val);             // 96，00=自动调光，01~FF 档位
QByteArray laneLightFrame(bool green);              // 99，false=红 true=绿
QByteArray yellowFlashFrame(bool on);               // 98
QByteArray queryContentFrame();                     // A0
QByteArray queryBrightFrame();                      // B6
QByteArray queryLaneFrame();                        // B9
QByteArray queryFlashFrame();                       // B8

struct Frame {
    quint8 cmd = 0;
    quint8 bright = 0;
    QByteArray data;
    bool bccOk = false;
    bool ok = false; // 帧形状合法且 BCC 校验通过
};

// 增量帧解析器：按长度字段提取完整治超帧并校验 BCC。
class FrameParser {
public:
    void feed(const QByteArray& chunk);
    bool next(Frame* out);

private:
    QByteArray m_buffer;
};

struct ContentReply {
    bool ok = false;      // A1~A8 且 BCC 通过
    int row = 0;          // 行索引 0~7
    QByteArray text;      // 16B 行数据（已去除尾部补空格）
    quint8 bright = 0;
};

struct StatusReply {
    bool ok = false;      // B6/B9/B8 且 BCC 通过
    quint8 value = 0;
};

ContentReply parseContentReply(const Frame& frame);
StatusReply parseStatusReply(const Frame& frame);

} // namespace sc_ol