#include "protocol/sichuan_ol/SiChuanOlProtocol.h"

#include "protocol/common/Codec.h"

namespace sc_ol {

namespace {

constexpr quint8 CMD_FULL_SCREEN = 0x80;
constexpr quint8 CMD_LINE_1      = 0x81;
constexpr quint8 CMD_LINE_8      = 0x88;
constexpr quint8 CMD_CLEAR       = 0x94;
constexpr quint8 CMD_BRIGHTNESS  = 0x96;
constexpr quint8 CMD_LANE        = 0x99;
constexpr quint8 CMD_FLASH       = 0x98;
constexpr quint8 CMD_QUERY_ALL   = 0xA0;
constexpr quint8 CMD_QUERY_BRIGHT= 0xB6;
constexpr quint8 CMD_QUERY_LANE  = 0xB9;
constexpr quint8 CMD_QUERY_FLASH = 0xB8;
constexpr quint8 REPLY_ROW_BASE  = 0xA1; // A1~A8 行内容应答

} // namespace

quint8 bcc(const QByteArray& raw)
{
    quint8 v = 0;
    const int end = qMax(0, raw.size() - 2); // 不含 BCC 与尾部 FF
    for (int i = 0; i < end; ++i)
        v ^= static_cast<quint8>(raw.at(i));
    return v;
}

QByteArray buildFrame(quint8 cmd, quint8 bright, const QByteArray& data)
{
    QByteArray frame;
    const int len = 6 + data.size(); // FF + len + cmd + bright + data + BCC + FF
    frame.reserve(len);
    frame.append(static_cast<char>(kHead));
    frame.append(static_cast<char>(len));
    frame.append(static_cast<char>(cmd));
    frame.append(static_cast<char>(bright));
    frame.append(data);
    // BCC：帧头(含)到数据段(含)全部字节异或（此时帧尚无 BCC/尾 FF，须取全量）。
    quint8 sum = 0;
    for (char c : frame)
        sum ^= static_cast<quint8>(c);
    frame.append(static_cast<char>(sum));
    frame.append(static_cast<char>(kTail));
    return frame;
}

QByteArray lineFrame(int row, const QString& text)
{
    const quint8 cmd = static_cast<quint8>(CMD_LINE_1 + qBound(0, row, kLineCount - 1));
    return buildFrame(cmd, kBrightMax, cd::gbkPadTo(cd::toGbk(text), kBytesPerLine));
}

QByteArray fullScreenFrame(const QString& text)
{
    QByteArray data = cd::toGbk(text);
    cd::gbkTruncate(data, kFullMaxBytes);
    return buildFrame(CMD_FULL_SCREEN, kBrightMax, data);
}

QByteArray clearFrame()
{
    // 文档示例 FF 07 94 00 00 6C FF：数据段恒 1 字节（00）
    return buildFrame(CMD_CLEAR, 0x00, QByteArray(1, static_cast<char>(0x00)));
}

QByteArray brightnessFrame(quint8 val)
{
    return buildFrame(CMD_BRIGHTNESS, 0x00, QByteArray(1, static_cast<char>(val)));
}

QByteArray laneLightFrame(bool green)
{
    return buildFrame(CMD_LANE, 0x00, QByteArray(1, static_cast<char>(green ? 0x01 : 0x00)));
}

QByteArray yellowFlashFrame(bool on)
{
    return buildFrame(CMD_FLASH, 0x00, QByteArray(1, static_cast<char>(on ? 0x01 : 0x00)));
}

QByteArray queryContentFrame()
{
    // 文档示例 FF 07 A0 00 00 58 FF：数据段恒 1 字节（00）
    return buildFrame(CMD_QUERY_ALL, 0x00, QByteArray(1, static_cast<char>(0x00)));
}

QByteArray queryBrightFrame()
{
    return buildFrame(CMD_QUERY_BRIGHT, 0x00, QByteArray(1, static_cast<char>(0x00)));
}

QByteArray queryLaneFrame()
{
    return buildFrame(CMD_QUERY_LANE, 0x00, QByteArray(1, static_cast<char>(0x00)));
}

QByteArray queryFlashFrame()
{
    return buildFrame(CMD_QUERY_FLASH, 0x00, QByteArray(1, static_cast<char>(0x00)));
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
        const int start = m_buffer.indexOf(static_cast<char>(kHead));
        if (start < 0) {
            m_buffer.clear();
            return false;
        }
        if (start > 0)
            m_buffer.remove(0, start);

        if (m_buffer.size() < 3)
            return false; // FF + len + cmd 尚未齐全

        const int len = static_cast<quint8>(m_buffer.at(1));
        if (len < kFrameLenMin || len > kFrameLenMax) {
            m_buffer.remove(0, 1); // 长度字段非法，丢弃该 0xFF 重扫
            continue;
        }

        if (m_buffer.size() < len)
            return false; // 等待剩余字节

        if (m_buffer.at(len - 1) != static_cast<char>(kTail)) {
            m_buffer.remove(0, 1); // 帧尾不符，重扫
            continue;
        }

        const QByteArray frame = m_buffer.left(len);
        m_buffer.remove(0, len);

        out->cmd    = static_cast<quint8>(frame.at(2));
        out->bright = static_cast<quint8>(frame.at(3));
        out->data   = frame.mid(4, len - 6);
        out->bccOk  = (bcc(frame) == static_cast<quint8>(frame.at(len - 2)));
        out->ok     = out->bccOk;
        return true;
    }
}

ContentReply parseContentReply(const Frame& frame)
{
    ContentReply r;
    if (!frame.ok)
        return r;
    if (frame.cmd < REPLY_ROW_BASE || frame.cmd >= REPLY_ROW_BASE + kLineCount)
        return r;
    if (frame.data.size() != kBytesPerLine)
        return r;

    r.ok     = true;
    r.row    = frame.cmd - REPLY_ROW_BASE;
    r.text   = frame.data;
    r.bright = frame.bright;
    while (!r.text.isEmpty() && static_cast<quint8>(r.text.at(r.text.size() - 1)) == 0x20)
        r.text.chop(1); // 去除尾部补空格
    return r;
}

StatusReply parseStatusReply(const Frame& frame)
{
    StatusReply r;
    if (!frame.ok)
        return r;
    if (frame.cmd != CMD_QUERY_BRIGHT && frame.cmd != CMD_QUERY_LANE &&
        frame.cmd != CMD_QUERY_FLASH)
        return r;
    if (frame.data.size() != 1)
        return r;

    r.ok    = true;
    r.value = static_cast<quint8>(frame.data.at(0));
    return r;
}

} // namespace sc_ol