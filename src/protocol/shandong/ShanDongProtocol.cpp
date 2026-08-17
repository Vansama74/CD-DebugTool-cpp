#include "protocol/shandong/ShanDongProtocol.h"

#include "protocol/common/Codec.h"

namespace shandong {

namespace {

constexpr char HEADER = static_cast<char>(0x7B); // '{'
constexpr char TAIL   = static_cast<char>(0x7D); // '}'

bool isPrintable(char c)
{
    const quint8 u = static_cast<quint8>(c);
    return u >= 0x20 && u <= 0x7E;
}

// GBK 编码文本，并将换行语义归一化：
//   "\r\n" → 0x0D 0x0A（协议 0x0D 被渲染引擎跳过、0x0A 换行）
//   "\n"   → 0x0A
//   "\r"   → 0x0D
QByteArray encodeTextWithNewlines(const QString &text)
{
    QByteArray out;
    int start = 0;
    const int n = text.size();
    for (int i = 0; i < n; ++i) {
        const QChar ch = text.at(i);
        if (ch == QLatin1Char('\n')) {
            out.append(cd::toGbk(text.mid(start, i - start)));
            out.append(static_cast<char>(0x0A));
            start = i + 1;
        } else if (ch == QLatin1Char('\r')) {
            out.append(cd::toGbk(text.mid(start, i - start)));
            out.append(static_cast<char>(0x0D));
            start = i + 1;
        }
    }
    out.append(cd::toGbk(text.mid(start)));
    return out;
}

} // namespace

QByteArray buildFrame(Cmd cmd, const QByteArray &payload)
{
    QByteArray frame;
    frame.reserve(payload.size() + 4);
    frame.append(HEADER);
    frame.append(static_cast<char>(cmd));
    frame.append(static_cast<char>(qBound(0, payload.size(), kMaxDataLen))); // 二进制长度
    frame.append(payload.left(kMaxDataLen));
    frame.append(TAIL);
    return frame;
}

QByteArray fillAllFrame(int color)
{
    // 协议原文：01红/02绿/03黄（二进制值，非 ASCII）。
    return buildFrame(Cmd::FillAll, QByteArray(1, static_cast<char>(qBound(1, color, 3))));
}

QByteArray versionFrame()
{
    // 协议示例参数 0x00。
    return buildFrame(Cmd::Version, QByteArray(1, static_cast<char>(0x00)));
}

QByteArray oneLineFrame(int color, int row, const QString &text)
{
    QByteArray p;
    p.reserve(2 + text.size() * 2);
    p.append(static_cast<char>('0' + qBound(0, color, 2)));
    p.append(static_cast<char>('1' + qBound(1, row, 5) - 1));
    p.append(cd::toGbk(text));
    return buildFrame(Cmd::OneLine, p);
}

QByteArray fullScreenFrame(int color, quint8 x, quint8 y, const QString &text)
{
    QByteArray p;
    p.reserve(3 + text.size() * 2);
    p.append(static_cast<char>('0' + qBound(0, color, 2)));
    p.append(static_cast<char>(x));
    p.append(static_cast<char>(y));
    p.append(encodeTextWithNewlines(text));
    return buildFrame(Cmd::FullScreen, p);
}

QByteArray clearFrame()
{
    return buildFrame(Cmd::Clear, QByteArray());
}

QByteArray brightnessFrame(int level)
{
    return buildFrame(Cmd::Brightness,
                      QByteArray(1, static_cast<char>('0' + qBound(0, level, 5))));
}

QByteArray peripheralFrame(bool green, bool red, bool yellow)
{
    quint8 ctrl = 0;
    if (green)
        ctrl |= 0x01U;
    if (red)
        ctrl |= 0x02U;
    if (yellow)
        ctrl |= 0x04U;
    return buildFrame(Cmd::Peripheral, QByteArray(1, static_cast<char>(ctrl)));
}

void ReplyScanner::feed(const QByteArray &chunk)
{
    m_buffer.append(chunk);
}

bool ReplyScanner::next(Reply *out)
{
    if (!out)
        return false;

    for (;;) {
        if (m_buffer.isEmpty())
            return false;

        // 可打印 ASCII 文本串（版本号应答无封套）。
        if (isPrintable(m_buffer.at(0))) {
            int end = 1;
            while (end < m_buffer.size() && isPrintable(m_buffer.at(end)))
                ++end;
            if (end < m_buffer.size()) { // 已碰到非可打印字节，串完整
                out->text  = QString::fromLatin1(m_buffer.left(end));
                out->valid = end >= 4; // 至少 4 字符才算文本应答
                m_buffer.remove(0, end);
                return true;
            }
            return false; // 串尚未结束，等待更多字节
        }

        m_buffer.remove(0, 1); // 不可打印杂散字节，丢弃
    }
}

bool ReplyScanner::flush(Reply *out)
{
    if (!out || m_buffer.isEmpty())
        return false;

    // 仅处理「可打印文本串延续到缓冲区末尾」的情况；其余内容丢弃。
    if (!isPrintable(m_buffer.at(0))) {
        m_buffer.clear();
        return false;
    }
    const int end = m_buffer.size();
    out->text  = QString::fromLatin1(m_buffer.left(end));
    out->valid = end >= 4;
    m_buffer.clear();
    return out->valid;
}

} // namespace shandong