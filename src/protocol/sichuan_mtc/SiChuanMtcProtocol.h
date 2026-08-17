#pragma once
#include <QByteArray>
#include <QString>
#include <QtGlobal>

// 四川 MTC 费显协议（1E 方案二，LED 点阵）——上位机发送侧与设备应答解析。
//
// '{' 帧族：'{' + 命令('1'~'9','A') + 参数 + [BCC] + '}'。
// BCC = 命令字(含)到参数(含)逐字节异或。固件不校验 BCC，同时兼容带/不带
// BCC 两种变体；本工具按协议文档默认携带 BCC。
//   '1' 初始化 / '2' 自检 / '5' 清屏：无参数
//   '3' 单行显示：行号('1'~'4') + 16B 数据（不足补 0x20）
//   '4' 全屏显示：64B 数据（不足补 0x20）
//   '6' 固定格式显示：X0('0'客车/'1'货车) + 客车 11B / 货车 20B ASCII
//   '7' 语音：'0'~'7' 固定语音；'8' 后跟 GBK 自定义文本
//   '8' 亮度('0'~'8'，0=自动) / '9' 音量('1'~'5') / 'A' 颜色('1'红'2'黄'3'绿)
// 7B 40~45 原始帧族（无 BCC）：40 改波特率 / 41 点阵大小 / 42 字体 /
// 43 协议类型 / 44 全屏点亮 / 45 获取版本号（应答 "SC_FX_P7.62_1.0"）。
// 0A 帧族：0A 46 0A 主机查询（应答 0A 64 0A 正常 / 0A 64 00 异常）；
// 0A 46 0D 主机清屏。
// 波特率 115200。
namespace sc_mtc {

enum class Cmd : char {
    Init         = '1',
    SelfCheck    = '2',
    OneLine      = '3',
    FullScreen   = '4',
    Clear        = '5',
    FixedDisplay = '6',
    Voice        = '7',
    Brightness   = '8',
    Volume       = '9',
    Color        = 'A',
};

constexpr int kBytesPerLine = 16; // 单行数据区 16B（8 汉字）
constexpr int kFullBytes    = 64; // 全屏数据区 64B（32 汉字）

// '{' 帧族：拼帧并计算 BCC（withBcc=false 时省略 BCC 字节）。
QByteArray bracesFrame(Cmd cmd, const QByteArray& params, bool withBcc = true);

QByteArray initFrame();                    // {1}
QByteArray selfCheckFrame();               // {2}
QByteArray clearFrame();                   // {5}
QByteArray oneLineFrame(int row, const QString& text);      // {3 行号 + 16B}
QByteArray fullScreenFrame(const QString& text);            // {4 64B}
// 固定显示：客车（车型 1~9 + 金额/余额各 5 位）；货车（总重/金额/余额/超重）。
QByteArray fixedBusFrame(int vehicleType, int amount, int balance);
QByteArray fixedTruckFrame(const QString& weightTons, int amount, int balance,
                           const QString& overTons);
QByteArray voiceFrame(int index);          // '0'~'7' 固定语音
QByteArray customVoiceFrame(const QString& text); // '8' 自定义 GBK 文本
QByteArray brightnessFrame(int level);     // '0'~'8'，0=自动
QByteArray volumeFrame(int level);         // '1'~'5'
QByteArray colorFrame(int color);          // '1'红 '2'黄 '3'绿

// 7B 40~45 原始帧族（无 BCC）。
QByteArray rawBaudFrame(int mode);         // 0=9600 / 1=115200
QByteArray rawDotSizeFrame(int size);      // 0=16 / 1=24 / 2=32 点阵
QByteArray rawFontFrame(int font);         // 0宋 1仿宋 2楷 3黑
QByteArray rawProtoFrame(int type);        // 0/2=治超屏协议，1=ETC协议
QByteArray rawFillAllFrame(int color);     // 0红 1绿 2黄 全屏点亮
QByteArray rawVersionFrame();              // 7B 45 7D

// 0A 帧族。
QByteArray hostQueryFrame();               // 0A 46 0A
QByteArray hostClearFrame();               // 0A 46 0D

struct Reply {
    enum Kind { None = 0, HostQuery, Text } kind = None;
    bool hostNormal = false; // HostQuery 应答：0A 64 0A 正常 / 0A 64 00 异常
    QString text;            // Text 应答（版本号等 ASCII 文本）
    bool valid = false;
};

// 应答扫描器：提取 0A 64 xx 0A 主机查询应答与 ASCII 文本串（版本号）。
class ReplyScanner {
public:
    void feed(const QByteArray& chunk);
    bool next(Reply* out);
    // 静默超时冲刷：把缓冲区尾部尚未终止的可打印文本串作为 Text 应答吐出
    // （版本号应答无封套且可能一次性到达，缺少终止字节时靠此兜底）。
    bool flush(Reply* out);

private:
    QByteArray m_buffer;
};

// 将重量字符串（"12.34" 吨）格式化为 5 位 ASCII（3 整数位 + 2 小数位）。
QByteArray weight5(const QString& tons);

} // namespace sc_mtc