#include "HexDumpPanel.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

namespace {

bool isHexString(const QString& s)
{
    if (s.isEmpty() || (s.size() % 2) != 0)
        return false;
    for (const QChar c : s) {
        const char ch = c.toLatin1();
        const bool ok = (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f')
                        || (ch >= 'A' && ch <= 'F');
        if (!ok)
            return false;
    }
    return true;
}

} // namespace

HexDumpPanel::HexDumpPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 4, 8, 8);
    layout->setSpacing(8);

    layout->addWidget(new QLabel(QStringLiteral("HEX:"), this));

    m_hexInput = new QLineEdit(this);
    m_hexInput->setPlaceholderText(
        QStringLiteral("输入十六进制数据，如 CC 01 01 21 ED DD"));
    connect(m_hexInput, &QLineEdit::returnPressed, this, &HexDumpPanel::onSend);
    layout->addWidget(m_hexInput, 1);

    m_autoChecksum = new QCheckBox(QStringLiteral("自动计算校验"), this);
    m_autoChecksum->setChecked(true);
    layout->addWidget(m_autoChecksum);

    m_sendBtn = new QPushButton(QStringLiteral("发送"), this);
    m_sendBtn->setObjectName(QStringLiteral("infoBtn"));
    m_sendBtn->setCursor(Qt::PointingHandCursor);
    connect(m_sendBtn, &QPushButton::clicked, this, &HexDumpPanel::onSend);
    layout->addWidget(m_sendBtn);
}

void HexDumpPanel::appendRx(const QString& hexLine)
{
    m_hexInput->setPlaceholderText(QStringLiteral("最近应答: %1").arg(hexLine));
}

void HexDumpPanel::onSend()
{
    const QString text = m_hexInput->text().trimmed();
    if (text.isEmpty())
        return;

    QString cleaned = text;
    cleaned.remove(QLatin1Char(' '));
    cleaned.remove(QLatin1Char(':'));
    cleaned.remove(QLatin1Char(','));

    if (!isHexString(cleaned))
        return;

    QByteArray data = QByteArray::fromHex(cleaned.toLatin1());

    // If auto-checksum is enabled and this looks like a 6-byte RS485 frame,
    // recompute the XOR checksum over the first 4 bytes into byte[4].
    if (m_autoChecksum->isChecked() && data.size() == 6) {
        quint8 x = 0;
        for (int i = 0; i < 4; ++i)
            x ^= static_cast<quint8>(data.at(i));
        data[4] = static_cast<char>(x);
    }

    emit sendHex(data);
    m_hexInput->clear();
}
