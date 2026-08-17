#include "DacScaleWidget.h"

#include "protocol/rs485/Rs485Commands.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>

DacScaleWidget::DacScaleWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* group = new QGroupBox(QStringLiteral("灯珠亮度系数 (2.8红 / 2.9绿, 1~40)"), this);
    auto* row = new QHBoxLayout(group);

    row->addWidget(new QLabel(QStringLiteral("红:"), group));
    m_redSpin = new QSpinBox(group);
    m_redSpin->setRange(Rs485Commands::DAC_SCALE_MIN, Rs485Commands::DAC_SCALE_MAX);
    m_redSpin->setValue(Rs485Commands::DAC_SCALE_RED_DEFAULT);
    m_redSpin->setToolTip(QStringLiteral("协议 0x09；转向与绿共用绿系数"));
    row->addWidget(m_redSpin);

    auto* setRedBtn = new QPushButton(QStringLiteral("设置红"), group);
    setRedBtn->setCursor(Qt::PointingHandCursor);
    connect(setRedBtn, &QPushButton::clicked, this, &DacScaleWidget::onSetRed);
    row->addWidget(setRedBtn);

    row->addSpacing(16);

    row->addWidget(new QLabel(QStringLiteral("绿:"), group));
    m_greenSpin = new QSpinBox(group);
    m_greenSpin->setRange(Rs485Commands::DAC_SCALE_MIN, Rs485Commands::DAC_SCALE_MAX);
    m_greenSpin->setValue(Rs485Commands::DAC_SCALE_GREEN_DEFAULT);
    row->addWidget(m_greenSpin);

    auto* setGreenBtn = new QPushButton(QStringLiteral("设置绿"), group);
    setGreenBtn->setCursor(Qt::PointingHandCursor);
    connect(setGreenBtn, &QPushButton::clicked, this, &DacScaleWidget::onSetGreen);
    row->addWidget(setGreenBtn);

    auto* applyBothBtn = new QPushButton(QStringLiteral("红绿同设"), group);
    applyBothBtn->setToolTip(
        QStringLiteral("绿系数对齐红系数，连续发送 0x09 与 0x0A"));
    applyBothBtn->setCursor(Qt::PointingHandCursor);
    connect(applyBothBtn, &QPushButton::clicked, this, &DacScaleWidget::onApplyBoth);
    row->addWidget(applyBothBtn);

    row->addStretch(1);
    layout->addWidget(group);
}

void DacScaleWidget::onSetRed()
{
    emit dacRedChanged(m_redSpin->value());
}

void DacScaleWidget::onSetGreen()
{
    emit dacGreenChanged(m_greenSpin->value());
}

void DacScaleWidget::onApplyBoth()
{
    const int v = m_redSpin->value();
    m_greenSpin->setValue(v);
    emit dacRedChanged(v);
    emit dacGreenChanged(v);
}

void DacScaleWidget::updateRed(int value)
{
    m_redSpin->setValue(qBound(Rs485Commands::DAC_SCALE_MIN, value, Rs485Commands::DAC_SCALE_MAX));
}

void DacScaleWidget::updateGreen(int value)
{
    m_greenSpin->setValue(qBound(Rs485Commands::DAC_SCALE_MIN, value, Rs485Commands::DAC_SCALE_MAX));
}
