#include "protocol/yunnan/YunNanProtocol.h"

#include <QTextCodec>
#include <QtMath>

namespace yunnan {

namespace {

constexpr char HEADER = static_cast<char>(0x7B); // '{'
constexpr char TAIL   = static_cast<char>(0x7D); // '}'

char asciiDigit(int value, int lo, int hi)
{
    const int clamped = qBound(lo, value, hi);
    return static_cast<char>('0' + clamped);
}

bool charToCmd(char c, Cmd* out)
{
    switch (static_cast<quint8>(c)) {
    case 0x01: *out = Cmd::FullScreenLight; return true;
    case 0x02: *out = Cmd::GetVersion;      return true;
    case '1': *out = Cmd::HostQuery;      return true;
    case '2': *out = Cmd::SelfCheck;      return true;
    case '3': *out = Cmd::OneLine;        return true;
    case '4': *out = Cmd::FullScreenEdit; return true;
    case '5': *out = Cmd::ClearAll;       return true;
    case '6': *out = Cmd::ClearLine;      return true;
    case '7': *out = Cmd::CivilVoice;     return true;
    case '8': *out = Cmd::Brightness;     return true;
    case '9': *out = Cmd::Volume;         return true;
    case 'A': *out = Cmd::Peripheral;     return true;
    case 'B': *out = Cmd::FeeVoice;       return true;
    default:  return false;
    }
}

} // namespace

QByteArray buildFrame(Cmd cmd, const QByteArray& payload)
{
    QByteArray frame;
    frame.reserve(payload.size() + 4);
    frame.append(HEADER);
    frame.append(static_cast<char>(cmd));
    frame.append(static_cast<char>(payload.size() & 0xFF));
    frame.append(payload);
    frame.append(TAIL);
    return frame;
}

QByteArray fullScreenLightPayload(int color)
{
    // 设备扩展七色：01红 02绿 03黄 04蓝 05紫 06青 07白。
    const int clamped = qBound(1, color, 7);
    return QByteArray(1, static_cast<char>(clamped));
}

QByteArray versionPayload()
{
    return QByteArray(1, static_cast<char>(0x00));
}

QByteArray oneLinePayload(int color, int row, const QString& text)
{
    QByteArray p;
    p.reserve(2 + text.size() * 2);
    p.append(asciiDigit(color, 0, 2));
    p.append(asciiDigit(row, 1, 5));
    p.append(toGbk(text));
    return p;
}

QByteArray fullScreenEditPayload(int color, quint8 x, quint8 y, const QString& text)
{
    QByteArray p;
    p.reserve(3 + text.size() * 2);
    p.append(asciiDigit(color, 0, 2));
    p.append(static_cast<char>(x));
    p.append(static_cast<char>(y));
    p.append(toGbk(text));
    return p;
}

QByteArray clearLinePayload(int row)
{
    return QByteArray(1, asciiDigit(row, 1, 5));
}

QByteArray civilVoicePayload(int index)
{
    return QByteArray(1, asciiDigit(index, 0, 3));
}

QByteArray brightnessPayload(int level)
{
    // 0 = 自动亮度（0x00 NUL）；1~8 = 手动档 ASCII '1'~'8'（8 最亮）。
    if (level <= 0)
        return QByteArray(1, static_cast<char>(0x00));
    return QByteArray(1, asciiDigit(level, 1, 8));
}

QByteArray volumePayload(int level)
{
    return QByteArray(1, asciiDigit(level, 1, 5));
}

QByteArray peripheralPayload(bool green, bool red, bool yellowFlash)
{
    quint8 ctrl = 0;
    if (green)       ctrl |= 0x01u;
    if (red)         ctrl |= 0x02u;
    if (yellowFlash) ctrl |= 0x04u;
    return QByteArray(1, static_cast<char>(ctrl));
}

QByteArray feeVoicePayload(const QString& amount)
{
    const QString s = amount.trimmed();
    bool ok = false;
    const double v = s.toDouble(&ok);
    // 金额 0 或非法 → 发送 "0"（设备侧 0 元不播报）。
    const QString text = (ok && v > 0.0) ? s : QStringLiteral("0");
    return text.toLatin1();
}

void FrameParser::feed(const QByteArray& chunk)
{
    m_buffer.append(chunk);
}

bool FrameParser::next(Frame* out)
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
            m_buffer.remove(0, start); // drop garbage before the header

        // Need at least: header + cmd + length.
        if (m_buffer.size() < 3)
            return false;

        const char cmdChar = m_buffer.at(1);
        const int len = static_cast<quint8>(m_buffer.at(2));
        const int total = 4 + len; // header + cmd + len + data + tail
        if (m_buffer.size() < total)
            return false; // wait for more bytes

        if (m_buffer.at(total - 1) != TAIL) {
            // Tail mismatch: drop the header and rescan for the next '{'.
            m_buffer.remove(0, 1);
            continue;
        }

        out->data = m_buffer.mid(3, len);
        out->ok = charToCmd(cmdChar, &out->cmd);
        m_buffer.remove(0, total);
        return true;
    }
}

QueryReply parseQueryReply(const QByteArray& frameData)
{
    QueryReply reply;

    QByteArray d = frameData;
    // Accept the full frame (7B 31 01 xx 7D) as well as the raw data field.
    if (d.size() >= 4 && static_cast<quint8>(d.at(0)) == 0x7B
        && static_cast<quint8>(d.at(d.size() - 1)) == 0x7D) {
        if (d.at(1) != '1')
            return reply;
        const int len = static_cast<quint8>(d.at(2));
        if (d.size() < 4 + len)
            return reply;
        d = d.mid(3, len);
    }

    if (d.size() != 1)
        return reply;

    reply.ok = true;
    reply.normal = (static_cast<quint8>(d.at(0)) == 0x00);
    return reply;
}

QString parseVersionReply(const QByteArray& bytes)
{
    QByteArray d = bytes;
    // Accept the framed reply (7B 02 len ... 7D) as well as bare ASCII text.
    if (d.size() >= 4 && static_cast<quint8>(d.at(0)) == 0x7B
        && static_cast<quint8>(d.at(d.size() - 1)) == 0x7D) {
        if (static_cast<quint8>(d.at(1)) != 0x02)
            return QString();
        const int len = static_cast<quint8>(d.at(2));
        if (d.size() < 4 + len)
            return QString();
        d = d.mid(3, len);
    }
    return fromGbk(d);
}

QByteArray toGbk(const QString& s)
{
    QTextCodec* codec = QTextCodec::codecForName("GBK");
    if (!codec)
        codec = QTextCodec::codecForLocale();
    if (codec)
        return codec->fromUnicode(s);
    return s.toUtf8();
}

QString fromGbk(const QByteArray& b)
{
    QTextCodec* codec = QTextCodec::codecForName("GBK");
    if (!codec)
        codec = QTextCodec::codecForLocale();
    if (codec)
        return codec->toUnicode(b);
    return QString::fromUtf8(b);
}

} // namespace yunnan