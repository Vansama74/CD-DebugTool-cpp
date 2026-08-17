#pragma once
#include <QByteArray>
#include <QString>
#include <QtGlobal>

// 山东车道费额显示器通信协议——上位机发送侧与设备应答解析。
//
// 帧格式（与青海协议同构，无校验）：
//   '{' (0x7B) + 命令字 (ASCII '1'~'5','7','8'，无 '6') + 参数长度 (1B 二进制)
//   + 参数[len] + '}' (0x7D)，帧总长 = len + 4。
//
// 固件权威实现：Project-STD-main/Application/Src/ProtocolParser_ShanDong/。
//   '1' 全屏单色：参数 1B 二进制 01红/02绿/03黄
//   '2' 取版本号：参数 1B（协议示例 0x00），设备裸 ASCII 应答产品程序编码
//        PROGRAM_CODE（如 "9K10212482"），无封套
//   '3' 单行显示：颜色 ASCII '0'~'2' + 行号 ASCII '1'~'5' + GBK 文本
//   '4' 全屏可编辑：颜色 ASCII '0'~'2' + X 二进制 + Y 二进制 + GBK 文本
//        （文本中 0x0A 换行 / 0x0D 被渲染引擎跳过）
//   '5' 清屏：无参数
//   '7' 亮度：ASCII '0'~'5'（0=自动调节）
//   '8' 外设：参数 1B 位图 bit0 绿灯 / bit1 红灯 / bit2 黄闪报警
// 除 '2' 外其余命令单向执行，设备不应答。
namespace shandong {

enum class Cmd : char {
    FillAll    = '1',
    Version    = '2',
    OneLine    = '3',
    FullScreen = '4',
    Clear      = '5',
    Brightness = '7',
    Peripheral = '8',
};

constexpr int kMaxDataLen = 255; // 长度字段为 1B 二进制 → 参数 ≤255

// 拼帧：'{' + 命令字 + 二进制长度 + 参数 + '}'（无校验）。
QByteArray buildFrame(Cmd cmd, const QByteArray &payload);

QByteArray fillAllFrame(int color); // '1'：1红/2绿/3黄（协议原文二进制值）
QByteArray versionFrame();          // '2'：参数 1B 0x00（协议示例）
// '3' 单行：color 0~2，row 1~5，文本 GBK 编码。
QByteArray oneLineFrame(int color, int row, const QString &text);
// '4' 全屏：color 0~2，x/y 二进制坐标，文本 GBK 编码。
// 文本中 '\r\n' 映射 0x0D 0x0A、'\n' 映射 0x0A（设备渲染引擎换行语义）。
QByteArray fullScreenFrame(int color, quint8 x, quint8 y, const QString &text);
QByteArray clearFrame();                     // '5'：无参数
QByteArray brightnessFrame(int level);       // '7'：'0'~'5'，0=自动调节
// '8' 外设：bit0 绿灯 / bit1 红灯 / bit2 黄闪报警。
QByteArray peripheralFrame(bool green, bool red, bool yellow);

struct Reply {
    bool valid = false;
    QString text; // 版本号等裸 ASCII 应答（无封套）
};

// 应答扫描器：提取可打印 ASCII 文本串（版本号应答 PROGRAM_CODE 无封套）。
class ReplyScanner {
public:
    void feed(const QByteArray &chunk);
    bool next(Reply *out);
    // 静默超时冲刷：把缓冲区尾部尚未终止的可打印文本串作为应答吐出
    // （版本号应答无封套且可能一次性到达，缺少终止字节时靠此兜底）。
    bool flush(Reply *out);

private:
    QByteArray m_buffer;
};

} // namespace shandong