#include "IntelHexParser.h"

#include <QList>
#include <QVector>

namespace {

// 单个 hex 字符 → 0~15；非法字符返回 -1。
int hexVal(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

// 校验和：记录内所有字节（长度/地址/类型/数据）之和 mod 256 == 0。
bool checksumOk(const QByteArray& raw)
{
    quint32 sum = 0;
    for (char c : raw)
        sum += static_cast<quint8>(c);
    return (sum & 0xFFu) == 0;
}

// 输出总跨度 sanity 上限：防恶意/异常 hex 按绝对地址展开导致内存爆炸 OOM。
// 正常固件 hex 的覆盖区间远小于 1 MiB（本项目固件约 0.23 MB），16 MiB 已
// 留足余量；超过即视为异常输入拒绝解析。
constexpr quint64 MAX_SPAN_BYTES = 16ull * 1024ull * 1024ull;

} // namespace

bool IntelHexParser::parse(const QByteArray& text, QByteArray* out, QString* error)
{
    out->clear();

    // 两遍式解析：
    //  第一遍：解析各记录并收集全部数据记录 (绝对地址, 数据)，04 记录按文件
    //    顺序更新基址、与各数据记录的绝对地址绑定；
    //  第二遍（遇 EOF 后）：取 [minAbs, maxEnd) 覆盖区间，裁剪为紧凑输出
    //    （偏移 = 绝对地址 - minAbs），区间内记录间间隙按 0xFF 填充。
    //
    // 不直接以绝对地址 resize：固件 hex 常带 04 高基址（如 0x08040000），旧
    // 实现按 `base + addr` 绝对展开会把输出膨胀到基址+末地址（本项目 ELF hex
    // 末条记录 end=0x080793A4 → 134,714,276 字节 ≈ 128 MB），加载/分包 OOM。
    struct Record {
        quint32 abs;      // 绝对地址（base + addr）
        QByteArray data;  // 数据记录载荷（不含 count/addr/type/checksum）
    };
    QVector<Record> records;
    quint32 base = 0;
    bool sawEof = false;

    const QList<QByteArray> lines = text.split('\n');
    int lineNo = 0;
    for (const QByteArray& lineRaw : lines) {
        ++lineNo;
        QByteArray line = lineRaw.trimmed(); // 去 \r 与首尾空白
        if (line.isEmpty())
            continue;
        if (line.at(0) != ':') {
            if (error)
                *error = QStringLiteral("第 %1 行缺少 ':' 前缀").arg(lineNo);
            return false;
        }
        line.remove(0, 1);
        if (line.isEmpty() || (line.size() % 2) != 0) {
            if (error)
                *error = QStringLiteral("第 %1 行 hex 长度非法").arg(lineNo);
            return false;
        }

        QByteArray raw;
        raw.reserve(line.size() / 2);
        for (int i = 0; i + 1 < line.size(); i += 2) {
            const int hi = hexVal(line.at(i));
            const int lo = hexVal(line.at(i + 1));
            if (hi < 0 || lo < 0) {
                if (error)
                    *error = QStringLiteral("第 %1 行含非法 hex 字符").arg(lineNo);
                return false;
            }
            raw.append(static_cast<char>((hi << 4) | lo));
        }

        if (!checksumOk(raw)) {
            if (error)
                *error = QStringLiteral("第 %1 行校验和错误").arg(lineNo);
            return false;
        }

        if (raw.size() < 5) {
            if (error)
                *error = QStringLiteral("第 %1 行记录过短").arg(lineNo);
            return false;
        }
        const int count = static_cast<quint8>(raw.at(0));
        const quint16 addr = static_cast<quint16>((static_cast<quint8>(raw.at(1)) << 8)
                                                  | static_cast<quint8>(raw.at(2)));
        const int type = static_cast<quint8>(raw.at(3));
        if (raw.size() != 5 + count) {
            if (error)
                *error = QStringLiteral("第 %1 行长度字段与实际不符").arg(lineNo);
            return false;
        }

        switch (type) {
        case 0x00: { // 数据记录：收集 (abs, data)，第二遍统一裁剪落位
            const quint32 abs = base + addr;
            const quint32 end = abs + static_cast<quint32>(count);
            if (end < abs) { // 地址回绕
                if (error)
                    *error = QStringLiteral("第 %1 行地址溢出").arg(lineNo);
                return false;
            }
            Record rec;
            rec.abs = abs;
            rec.data = QByteArray(raw.constData() + 4, count);
            records.append(std::move(rec));
            break;
        }
        case 0x01: // 文件结束
            sawEof = true;
            break;
        case 0x02: // 扩展段地址（base = 段地址 << 4）：与 04 仅移位量不同（<< 4
                   // vs << 16，段 ×16 vs 段 ×65536），长度/校验和检查与 04 一致。
                   // GNU objcopy -I binary -O ihex 以 02 记录表达 64KiB 段边界。
            if (count != 2) {
                if (error)
                    *error = QStringLiteral("第 %1 行扩展段地址长度应为 2").arg(lineNo);
                return false;
            }
            base = static_cast<quint32>((static_cast<quint8>(raw.at(4)) << 8)
                                        | static_cast<quint8>(raw.at(5)))
                   << 4;
            break;
        case 0x04: // 扩展线性地址（base = 高 16 位 << 16）
            if (count != 2) {
                if (error)
                    *error = QStringLiteral("第 %1 行扩展线性地址长度应为 2").arg(lineNo);
                return false;
            }
            base = static_cast<quint32>((static_cast<quint8>(raw.at(4)) << 8)
                                        | static_cast<quint8>(raw.at(5)))
                   << 16;
            break;
        case 0x05: // 起始线性地址：仅作元数据，忽略
            break;
        default:
            if (error)
                *error = QStringLiteral("第 %1 行不支持记录类型 0x%2")
                             .arg(lineNo)
                             .arg(type, 2, 16, QLatin1Char('0'));
            return false;
        }
    }

    if (!sawEof) {
        if (error)
            *error = QStringLiteral("缺少 EOF 记录 (类型 01)");
        return false;
    }
    if (records.isEmpty()) {
        if (error)
            *error = QStringLiteral("HEX 文件不含数据记录");
        return false;
    }

    // 第二遍：求覆盖区间 [minAbs, maxEnd) 并裁剪。各数据记录在收集时已做
    // abs + count 回绕检查，此处同算术不可能回绕。
    quint32 minAbs = records.first().abs;
    quint32 maxEnd = 0;
    for (const Record& rec : records) {
        minAbs = qMin(minAbs, rec.abs);
        maxEnd = qMax(maxEnd, rec.abs + static_cast<quint32>(rec.data.size()));
    }
    const quint64 span = static_cast<quint64>(maxEnd) - minAbs;
    if (span > MAX_SPAN_BYTES) {
        if (error)
            *error = QStringLiteral("HEX 地址跨度 %1 字节超过 16 MiB 上限，拒绝解析")
                         .arg(span);
        return false;
    }

    out->resize(static_cast<int>(span));
    out->fill(static_cast<char>(0xFF)); // 记录间间隙按 0xFF（flash 擦除态语义）
    for (const Record& rec : records)
        memcpy(out->data() + static_cast<int>(rec.abs - minAbs), rec.data.constData(),
               static_cast<size_t>(rec.data.size()));
    return true;
}