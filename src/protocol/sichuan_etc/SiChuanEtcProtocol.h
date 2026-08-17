#pragma once
#include <QByteArray>
#include <QString>
#include <QtGlobal>

// 四川 ETC 费显协议（1D）——上位机发送侧与设备应答解析。
//
// 帧格式：0x0A + 命令位 + 参数 + 0x0D。
//   * 静态显示：0A 00 <行号 0~6> <GBK 数据> 0D（0=全屏 ≤56B，1~6 行 ≤24B）
//   * 清屏：0A 00 <行号> 20 0D（0=全屏，1~6 行）
//   * 初始化：0A 00 00 30 0D（数据首字节 0x30，设备执行复位）
//   * 亮度：0A 40 <0~7> 00 0D（固件语义；上位机以固件 app_sc_etc_proto.c 为准）
//   * 交通灯红/绿：0A 36/37 0D；黄闪开/关：0A 38/39 0D
//   * 设备应答：0A 00/01/02 0D（正常 / 数据超长 / 帧错误）
//
// DebugTool 扩展（保留用于固件调试）：
//   * 滚屏 0A 01 00 md rt st 数据 0D；心跳 0A 50 0D
// 波特率 115200（设备拨码 2 置 ON 时为 9600）。
namespace sc_etc {

// 显示命令数据长度上限：全屏 56B（28 汉字）/ 单行 24B（12 汉字）。
constexpr int kLineMaxBytes   = 24;
constexpr int kFullMaxBytes   = 56;
constexpr int kScrollMaxBytes = 50; // 滚屏数据（DebugTool 扩展，固件上限）

enum class Light : char {
    Red       = 0x36,
    Green     = 0x37,
    YellowOn  = 0x38,
    YellowOff = 0x39,
};

// 静态显示帧。row：0=全屏，1~6=第 1~6 行；文本 GBK，超长按上限截断。
QByteArray displayFrame(int row, const QString& text);
// 滚屏帧（DebugTool 扩展）。md：00 静态 / 01 上滚 / 03 左滚（其余保留）；
// rt 移屏秒数；st 停留秒数（255 永远停留）。
QByteArray scrollFrame(quint8 md, quint8 rt, quint8 st, const QString& text);
// 清屏帧：0A 00 <行号> 20 0D；row 0=全屏，1~6 行。
QByteArray clearFrame(int row);
// 初始化帧（数据首字节 0x30，设备执行复位）。
QByteArray initFrame();
// 灯控帧：交通灯红/绿、黄闪开/关。
QByteArray lightFrame(Light light);
// 亮度帧：0A 40 <0~7> 00 0D（固件语义，0=自动调光）。
QByteArray brightnessFrame(quint8 level);
// 心跳帧（DebugTool 扩展；保活，设备不回）。
QByteArray heartbeatFrame();

struct AckReply {
    enum Kind { Unknown = 0, Ok = 1, TooLong = 2, FrameError = 3 };
    Kind kind = Unknown;
    bool valid = false; // 提取到合法应答帧（命令位为 00/01/02）
};

// 应答扫描器：从字节流中提取 `0A XX 0D` 应答帧。
class AckScanner {
public:
    void feed(const QByteArray& chunk);
    bool next(AckReply* out);

private:
    QByteArray m_buffer;
};

} // namespace sc_etc