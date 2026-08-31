#pragma once
#include <QByteArray>
#include <QString>

// Intel HEX 固件文件解析（记录类型 00 数据 / 01 文件结束 / 02 扩展段地址 /
// 04 扩展线性地址 / 05 起始线性地址）。解析结果为「紧凑」字节流：输出按覆盖区间
// [minAbs, maxEnd) 裁剪（偏移 = 绝对地址 - minAbs），与 .bin 字节流等价，
// 可直接进入既有固件加载/分包路径（UpgradeEngine::loadFirmware）。
class IntelHexParser {
public:
    // 解析失败返回 false，error 携带原因（坏校验和 / 未知记录类型 / 缺少 EOF /
    // 无数据记录 / 地址跨度超过 16 MiB sanity 上限等）。
    // 成功时 out 为区间 [minAbs, maxEnd) 的连续字节流；记录间的地址间隙按
    // 0xFF 填充——对齐设备 flash 擦除态与固件 CRC 的 0xFF 填充语义
    // （Recovery 4B04 擦除后未覆盖区域即 0xFF）。
    // 注意：不带 04 高基址的 hex（base-0）与原语义一致；带 04 高基址
    // （如固件 ELF hex 的 0x08040000）不再按绝对地址展开（旧行为会把
    // 输出膨胀到 100+ MB）。
    static bool parse(const QByteArray& text, QByteArray* out, QString* error);
};