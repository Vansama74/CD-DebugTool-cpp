#include "LedIndicator.h"

#include <QPainter>
#include <QPalette>

LedIndicator::LedIndicator(const QString& text, const QColor& color, int size, QWidget* parent)
    : QWidget(parent)
    , m_text(text)
    , m_color(color)
    , m_size(size)
{
    setFixedSize(size + 60, size + 10);
}

void LedIndicator::setColor(const QColor& color)
{
    if (m_color == color)
        return;
    m_color = color;
    update();
}

void LedIndicator::setText(const QString& text)
{
    if (m_text == text)
        return;
    m_text = text;
    update();
}

void LedIndicator::setActive(bool active)
{
    if (m_active == active)
        return;
    m_active = active;
    update();
}

void LedIndicator::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const int yOffset = (height() - m_size) / 2;

    // Circle: lit color when active, muted gray otherwise.
    painter.setBrush(m_active ? m_color : QColor(QStringLiteral("#30363D")));
    painter.setPen(QColor(QStringLiteral("#21262D")));
    painter.drawEllipse(5, yOffset, m_size, m_size);

    // Label.
    painter.setPen(palette().color(QPalette::WindowText));
    painter.drawText(m_size + 10, yOffset + m_size - 4, m_text);
}
