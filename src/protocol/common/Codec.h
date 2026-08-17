#pragma once
#include <QByteArray>
#include <QString>
#include <QTextCodec>

// 费显协议公共工具：GBK 编解码与定长数据区适配。
//
// 四川 ETC / MTC / 治超屏三个协议的显示文本均以 GBK 传输（汉字高位在前），
// 行/屏数据区为定长字节（不足补 0x20，超出截断）。截断必须避免切开双字节
// 汉字：GBK 前导字节范围 0x81~0xFE，后跟尾字节 0x40~0x7E / 0x80~0xFE。
namespace cd {

inline QByteArray toGbk(const QString& s)
{
    QTextCodec* codec = QTextCodec::codecForName("GBK");
    if (!codec)
        codec = QTextCodec::codecForLocale();
    if (codec)
        return codec->fromUnicode(s);
    return s.toUtf8();
}

inline QString fromGbk(const QByteArray& b)
{
    QTextCodec* codec = QTextCodec::codecForName("GBK");
    if (!codec)
        codec = QTextCodec::codecForLocale();
    if (codec)
        return codec->toUnicode(b);
    return QString::fromUtf8(b);
}

// 按最大长度安全截断 GBK 字节串（避免切开双字节汉字）。
// 从字符串起点扫描字符边界：ASCII 单字节；GBK 汉字为前导字节(0x81~0xFE)
// + 尾字节(0x40~0x7E / 0x80~0xFE)。toGbk 产出的字节串必然良构，故扫描可靠。
inline void gbkTruncate(QByteArray& b, int maxLen)
{
    if (b.size() <= maxLen)
        return;

    int boundary = 0; // 不超过 maxLen 的最后一个完整字符边界
    const int n = b.size();
    for (int i = 0; i < n; ++i) {
        const quint8 c = static_cast<quint8>(b.at(i));
        if (c >= 0x81 && c <= 0xFE)
            ++i; // 双字节字符：连同尾字节跳过
        if (i + 1 <= maxLen)
            boundary = i + 1;
        else
            break;
    }
    b.truncate(boundary);
}

// 将 GBK 字节串适配为定长 len：超长截断，不足补空格 0x20。
inline QByteArray gbkPadTo(QByteArray b, int len)
{
    gbkTruncate(b, len);
    while (b.size() < len)
        b.append(static_cast<char>(0x20));
    return b;
}

} // namespace cd