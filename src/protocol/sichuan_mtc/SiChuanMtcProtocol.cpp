#include "protocol/sichuan_mtc/SiChuanMtcProtocol.h"

#include "protocol/common/Codec.h"

namespace sc_mtc {

namespace {

constexpr char HEADER = static_cast<char>(0x7B); // '{'
constexpr char TAIL   = static_cast<char>(0x7D); // '}'

char asciiDigit(int value, int lo, int hi)
{
    const int clamped = qBound(lo, value, hi);
    return static_cast<char>('0' + clamped);
}

QByteArray digits5(int value)
{
    return QString::number(qBound(0, value, 99999))
        .rightJustified(5, QLatin1Char('0'))
        .toLatin1();
}

bool isPrintable(char c)
{
    const quint8 u = static_cast<quint8>(c);
    return u >= 0x20 && u <= 0x7E;
}

} // namespace

QByteArray bracesFrame(Cmd cmd, const QByteArray& params, bool withBcc)
{
    QByteArray frame;
    frame.reserve(params.size() + (withBcc ? 4 : 3));
    frame.append(HEADER);
    frame.append(static_cast<char>(cmd));
    frame.append(params);
    if (withBcc) {
        char bcc = static_cast<char>(cmd); // 命令字(含)到参数(含)异或
        for (char c : params)
            bcc ^= c;
        frame.append(bcc);
    }
    frame.append(TAIL);
    return frame;
}

QByteArray initFrame()
{
    return bracesFrame(Cmd::Init, QByteArray());
}

QByteArray selfCheckFrame()
{
    return bracesFrame(Cmd::SelfCheck, QByteArray());
}

QByteArray clearFrame()
{
    return bracesFrame(Cmd::Clear, QByteArray());
}

QByteArray oneLineFrame(int row, const QString& text)
{
    QByteArray p;
    p.reserve(1 + kBytesPerLine);
    p.append(asciiDigit(row, 1, 4));
    p.append(cd::gbkPadTo(cd::toGbk(text), kBytesPerLine));
    return bracesFrame(Cmd::OneLine, p);
}

QByteArray fullScreenFrame(const QString& text)
{
    return bracesFrame(Cmd::FullScreen, cd::gbkPadTo(cd::toGbk(text), kFullBytes));
}

QByteArray fixedBusFrame(int vehicleType, int amount, int balance)
{
    QByteArray p;
    p.reserve(12);
    p.append('0'); // 客车
    p.append(asciiDigit(vehicleType, 1, 9)); // X1 车型
    p.append(digits5(amount));               // X2~X6 金额
    p.append(digits5(balance));              // X7~X11 余额
    return bracesFrame(Cmd::FixedDisplay, p);
}

QByteArray fixedTruckFrame(const QString& weightTons, int amount, int balance,
                           const QString& overTons)
{
    QByteArray p;
    p.reserve(21);
    p.append('1');            // 货车
    p.append(weight5(weightTons)); // X1~X5 总重
    p.append(digits5(amount));     // X6~X10 金额
    p.append(digits5(balance));    // X11~X15 余额
    p.append(weight5(overTons));   // X16~X20 超重
    return bracesFrame(Cmd::FixedDisplay, p);
}

QByteArray voiceFrame(int index)
{
    return bracesFrame(Cmd::Voice, QByteArray(1, asciiDigit(index, 0, 7)));
}

QByteArray customVoiceFrame(const QString& text)
{
    QByteArray p;
    p.reserve(1 + text.size() * 2);
    p.append('8');
    p.append(cd::toGbk(text));
    return bracesFrame(Cmd::Voice, p);
}

QByteArray brightnessFrame(int level)
{
    return bracesFrame(Cmd::Brightness, QByteArray(1, asciiDigit(level, 0, 8)));
}

QByteArray volumeFrame(int level)
{
    return bracesFrame(Cmd::Volume, QByteArray(1, asciiDigit(level, 1, 5)));
}

QByteArray colorFrame(int color)
{
    return bracesFrame(Cmd::Color, QByteArray(1, asciiDigit(color, 1, 3)));
}

QByteArray rawBaudFrame(int mode)
{
    // 9600: 7B 40 00 25 80 7D；115200: 7B 40 01 C2 00 7D
    QByteArray f;
    f.append(HEADER);
    f.append(static_cast<char>(0x40));
    f.append(static_cast<char>(mode ? 0x01 : 0x00));
    f.append(static_cast<char>(mode ? 0xC2 : 0x25));
    f.append(static_cast<char>(mode ? 0x00 : 0x80));
    f.append(TAIL);
    return f;
}

QByteArray rawDotSizeFrame(int size)
{
    QByteArray f;
    f.append(HEADER);
    f.append(static_cast<char>(0x41));
    f.append(static_cast<char>(qBound(0, size, 2)));
    f.append(TAIL);
    return f;
}

QByteArray rawFontFrame(int font)
{
    QByteArray f;
    f.append(HEADER);
    f.append(static_cast<char>(0x42));
    f.append(static_cast<char>(qBound(0, font, 3)));
    f.append(TAIL);
    return f;
}

QByteArray rawProtoFrame(int type)
{
    QByteArray f;
    f.append(HEADER);
    f.append(static_cast<char>(0x43));
    f.append(static_cast<char>(qBound(0, type, 2)));
    f.append(TAIL);
    return f;
}

QByteArray rawFillAllFrame(int color)
{
    QByteArray f;
    f.append(HEADER);
    f.append(static_cast<char>(0x44));
    f.append(static_cast<char>(qBound(0, color, 2)));
    f.append(TAIL);
    return f;
}

QByteArray rawVersionFrame()
{
    QByteArray f;
    f.append(HEADER);
    f.append(static_cast<char>(0x45));
    f.append(TAIL);
    return f;
}

QByteArray hostQueryFrame()
{
    return QByteArray::fromHex("0a460a");
}

QByteArray hostClearFrame()
{
    return QByteArray::fromHex("0a460d");
}

void ReplyScanner::feed(const QByteArray& chunk)
{
    m_buffer.append(chunk);
}

bool ReplyScanner::next(Reply* out)
{
    if (!out)
        return false;

    for (;;) {
        if (m_buffer.isEmpty())
            return false;

        // 主机查询应答：0A 64 xx 0A（3 字节）。
        if (static_cast<quint8>(m_buffer.at(0)) == 0x0A) {
            if (m_buffer.size() >= 3 && m_buffer.at(1) == static_cast<char>(0x64) &&
                (m_buffer.at(2) == static_cast<char>(0x0A) ||
                 m_buffer.at(2) == static_cast<char>(0x00))) {
                out->kind       = Reply::HostQuery;
                out->hostNormal = (m_buffer.at(2) == static_cast<char>(0x0A));
                out->valid      = true;
                m_buffer.remove(0, 3);
                return true;
            }
            m_buffer.remove(0, 1); // 非查询应答的 0A，丢弃重扫
            continue;
        }

        // 可打印 ASCII 文本串（版本号应答无封套）。
        if (isPrintable(m_buffer.at(0))) {
            int end = 1;
            while (end < m_buffer.size() && isPrintable(m_buffer.at(end)))
                ++end;
            if (end < m_buffer.size()) { // 已碰到非可打印字节，串完整
                out->kind  = Reply::Text;
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

bool ReplyScanner::flush(Reply* out)
{
    if (!out || m_buffer.isEmpty())
        return false;

    // 仅处理「可打印文本串延续到缓冲区末尾」的情况；其余内容丢弃。
    if (!isPrintable(m_buffer.at(0))) {
        m_buffer.clear();
        return false;
    }
    const int end = m_buffer.size();
    out->kind  = Reply::Text;
    out->text  = QString::fromLatin1(m_buffer.left(end));
    out->valid = end >= 4;
    m_buffer.clear();
    return out->valid;
}

QByteArray weight5(const QString& tons)
{
    const QString s = tons.trimmed();
    const int dot   = s.indexOf(QLatin1Char('.'));
    const QString intPart  = (dot < 0) ? s : s.left(dot);
    const QString fracPart = (dot < 0) ? QString() : s.mid(dot + 1).left(2);

    const int iv = qBound(0, intPart.toInt(), 999);
    return QString::number(iv).rightJustified(3, QLatin1Char('0')).toLatin1() +
           fracPart.leftJustified(2, QLatin1Char('0')).toLatin1();
}

} // namespace sc_mtc