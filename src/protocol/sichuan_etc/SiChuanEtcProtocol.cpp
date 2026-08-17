#include "protocol/sichuan_etc/SiChuanEtcProtocol.h"

#include "protocol/common/Codec.h"

namespace sc_etc {

namespace {

constexpr char HEADER = static_cast<char>(0x0A);
constexpr char TAIL   = static_cast<char>(0x0D);

// GBK 文本按上限截断（安全截断，不切开汉字）。
QByteArray fitGbk(const QString& text, int maxBytes)
{
    QByteArray gbk = cd::toGbk(text);
    cd::gbkTruncate(gbk, maxBytes);
    return gbk;
}

} // namespace

QByteArray displayFrame(int row, const QString& text)
{
    QByteArray frame;
    const int maxBytes = (row == 0) ? kFullMaxBytes : kLineMaxBytes;
    frame.reserve(4 + maxBytes);
    frame.append(HEADER);
    frame.append(static_cast<char>(0x00));              // 静态显示
    frame.append(static_cast<char>(qBound(0, row, 6))); // 行号 0~6
    frame.append(fitGbk(text, maxBytes));
    frame.append(TAIL);
    return frame;
}

QByteArray scrollFrame(quint8 md, quint8 rt, quint8 st, const QString& text)
{
    QByteArray frame;
    frame.reserve(7 + kScrollMaxBytes);
    frame.append(HEADER);
    frame.append(static_cast<char>(0x01)); // 滚屏显示
    frame.append(static_cast<char>(0x00)); // 行号固定 0
    frame.append(static_cast<char>(md));
    frame.append(static_cast<char>(rt));
    frame.append(static_cast<char>(st));
    frame.append(fitGbk(text, kScrollMaxBytes));
    frame.append(TAIL);
    return frame;
}

QByteArray clearFrame(int row)
{
    QByteArray frame;
    frame.append(HEADER);
    frame.append(static_cast<char>(0x00)); // 静态显示
    frame.append(static_cast<char>(qBound(0, row, 6)));
    frame.append(static_cast<char>(0x20)); // 数据首字节 0x20 → 清屏
    frame.append(TAIL);
    return frame;
}

QByteArray initFrame()
{
    QByteArray frame;
    frame.append(HEADER);
    frame.append(static_cast<char>(0x00));
    frame.append(static_cast<char>(0x00));
    frame.append(static_cast<char>(0x30)); // 数据首字节 0x30 → 初始化（复位）
    frame.append(TAIL);
    return frame;
}

QByteArray lightFrame(Light light)
{
    QByteArray frame;
    frame.append(HEADER);
    frame.append(static_cast<char>(light));
    frame.append(TAIL);
    return frame;
}

QByteArray brightnessFrame(quint8 level)
{
    QByteArray frame;
    frame.append(HEADER);
    frame.append(static_cast<char>(0x40));
    frame.append(static_cast<char>(qBound<quint8>(0, level, 7)));
    frame.append(static_cast<char>(0x00)); // 保留位
    frame.append(TAIL);
    return frame;
}

QByteArray heartbeatFrame()
{
    QByteArray frame;
    frame.append(HEADER);
    frame.append(static_cast<char>(0x50));
    frame.append(TAIL);
    return frame;
}

void AckScanner::feed(const QByteArray& chunk)
{
    m_buffer.append(chunk);
}

bool AckScanner::next(AckReply* out)
{
    if (!out)
        return false;

    for (;;) {
        const int start = m_buffer.indexOf(HEADER);
        if (start < 0) {
            m_buffer.clear();
            return false;
        }
        if (start > 0)
            m_buffer.remove(0, start);

        if (m_buffer.size() < 3)
            return false;

        if (m_buffer.at(2) != TAIL) {
            m_buffer.remove(0, 1); // 非应答帧形状，丢弃帧头重扫
            continue;
        }

        const quint8 code = static_cast<quint8>(m_buffer.at(1));
        out->valid = (code == 0x00 || code == 0x01 || code == 0x02);
        switch (code) {
        case 0x00: out->kind = AckReply::Ok;         break;
        case 0x01: out->kind = AckReply::TooLong;    break;
        case 0x02: out->kind = AckReply::FrameError; break;
        default:   out->kind = AckReply::Unknown;    break;
        }
        m_buffer.remove(0, 3);
        return true;
    }
}

} // namespace sc_etc