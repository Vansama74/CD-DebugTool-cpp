#include "DisplayWidget.h"

#include "LedIndicator.h"

#include <QColor>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace {

QColor ledColorForState(quint8 state)
{
    switch (state) {
    case 0x01:
    case 0x10:
        return QColor(QStringLiteral("#F38BA8")); // red
    case 0x02:
    case 0x20:
        return QColor(QStringLiteral("#A6E3A1")); // green
    case 0x03:
    case 0x30:
        return QColor(QStringLiteral("#F9E2AF")); // yellow (turn)
    default:
        return QColor(QStringLiteral("#30363D")); // gray (off)
    }
}

void addLed(QVBoxLayout* layout, QHash<quint8, LedIndicator*>& map, quint8 key,
            const QString& text)
{
    auto* led = new LedIndicator(text, ledColorForState(key));
    layout->addWidget(led);
    map.insert(key, led);
}

} // namespace

DisplayWidget::DisplayWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(12);

    // Front column.
    auto* frontGroup = new QFrame(this);
    frontGroup->setFrameShape(QFrame::StyledPanel);
    auto* frontLayout = new QVBoxLayout(frontGroup);
    auto* frontTitle = new QLabel(QStringLiteral("正面"), frontGroup);
    frontTitle->setAlignment(Qt::AlignCenter);
    frontTitle->setStyleSheet(QStringLiteral("font-weight: bold; color: #58A6FF;"));
    frontLayout->addWidget(frontTitle);
    addLed(frontLayout, m_frontLeds, 0x00, QStringLiteral("关闭"));
    addLed(frontLayout, m_frontLeds, 0x10, QStringLiteral("红"));
    addLed(frontLayout, m_frontLeds, 0x20, QStringLiteral("绿"));
    addLed(frontLayout, m_frontLeds, 0x30, QStringLiteral("转"));
    frontLayout->addStretch(1);
    layout->addWidget(frontGroup);

    // Back column.
    auto* backGroup = new QFrame(this);
    backGroup->setFrameShape(QFrame::StyledPanel);
    auto* backLayout = new QVBoxLayout(backGroup);
    auto* backTitle = new QLabel(QStringLiteral("背面"), backGroup);
    backTitle->setAlignment(Qt::AlignCenter);
    backTitle->setStyleSheet(QStringLiteral("font-weight: bold; color: #58A6FF;"));
    backLayout->addWidget(backTitle);
    addLed(backLayout, m_backLeds, 0x00, QStringLiteral("关闭"));
    addLed(backLayout, m_backLeds, 0x01, QStringLiteral("红"));
    addLed(backLayout, m_backLeds, 0x02, QStringLiteral("绿"));
    addLed(backLayout, m_backLeds, 0x03, QStringLiteral("转"));
    backLayout->addStretch(1);
    layout->addWidget(backGroup);
}

void DisplayWidget::updateState(quint8 front, quint8 back)
{
    for (auto it = m_frontLeds.constBegin(); it != m_frontLeds.constEnd(); ++it)
        it.value()->setActive(it.key() == front);
    for (auto it = m_backLeds.constBegin(); it != m_backLeds.constEnd(); ++it)
        it.value()->setActive(it.key() == back);
}

void DisplayWidget::clear()
{
    for (auto it = m_frontLeds.constBegin(); it != m_frontLeds.constEnd(); ++it)
        it.value()->setActive(false);
    for (auto it = m_backLeds.constBegin(); it != m_backLeds.constEnd(); ++it)
        it.value()->setActive(false);
}
