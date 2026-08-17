#include "BrightnessWidget.h"

#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {
constexpr int BRIGHTNESS_AUTO = 0xFF;
}

BrightnessWidget::BrightnessWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    // Brightness group.
    auto* brightnessGroup = new QGroupBox(QStringLiteral("亮度调节"), this);
    auto* brightnessLayout = new QVBoxLayout(brightnessGroup);

    auto* modeRow = new QHBoxLayout();
    modeRow->addWidget(new QLabel(QStringLiteral("调光模式:"), brightnessGroup));
    m_modeCombo = new QComboBox(brightnessGroup);
    m_modeCombo->addItem(QStringLiteral("手动调光"), 0);
    m_modeCombo->addItem(QStringLiteral("自动调光"), 1);
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BrightnessWidget::onModeChanged);
    modeRow->addWidget(m_modeCombo);
    modeRow->addStretch(1);
    brightnessLayout->addLayout(modeRow);

    auto* sliderRow = new QHBoxLayout();
    m_slider = new QSlider(Qt::Horizontal, brightnessGroup);
    m_slider->setRange(0, 100);
    m_slider->setValue(80);
    m_slider->setTickPosition(QSlider::TicksBelow);
    m_slider->setTickInterval(10);
    connect(m_slider, &QSlider::valueChanged, this, &BrightnessWidget::onSliderChanged);
    sliderRow->addWidget(m_slider, 1);

    m_spin = new QSpinBox(brightnessGroup);
    m_spin->setRange(0, 100);
    m_spin->setValue(80);
    m_spin->setSuffix(QStringLiteral("%"));
    connect(m_spin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &BrightnessWidget::onSpinChanged);
    sliderRow->addWidget(m_spin);
    brightnessLayout->addLayout(sliderRow);

    auto* btnRow = new QHBoxLayout();
    m_setBtn = new QPushButton(QStringLiteral("设置亮度"), brightnessGroup);
    m_setBtn->setObjectName(QStringLiteral("primaryBtn"));
    m_setBtn->setCursor(Qt::PointingHandCursor);
    connect(m_setBtn, &QPushButton::clicked, this, &BrightnessWidget::onSetBrightness);
    btnRow->addWidget(m_setBtn);

    m_queryBtn = new QPushButton(QStringLiteral("查询亮度"), brightnessGroup);
    m_queryBtn->setCursor(Qt::PointingHandCursor);
    connect(m_queryBtn, &QPushButton::clicked, this, &BrightnessWidget::queryBrightnessRequested);
    btnRow->addWidget(m_queryBtn);
    btnRow->addStretch(1);
    brightnessLayout->addLayout(btnRow);

    layout->addWidget(brightnessGroup);

    // Auto-dimming range group (enabled only in auto mode).
    m_rangeGroup = new QGroupBox(QStringLiteral("自动调光范围"), this);
    m_rangeGroup->setEnabled(false);
    auto* rangeLayout = new QVBoxLayout(m_rangeGroup);

    auto* minRow = new QHBoxLayout();
    minRow->addWidget(new QLabel(QStringLiteral("最小值:"), m_rangeGroup));
    m_minSpin = new QSpinBox(m_rangeGroup);
    m_minSpin->setRange(0, 100);
    m_minSpin->setValue(67);
    m_minSpin->setSuffix(QStringLiteral("%"));
    minRow->addWidget(m_minSpin);
    auto* setMinBtn = new QPushButton(QStringLiteral("设置"), m_rangeGroup);
    setMinBtn->setCursor(Qt::PointingHandCursor);
    connect(setMinBtn, &QPushButton::clicked, this, &BrightnessWidget::onSetMin);
    minRow->addWidget(setMinBtn);
    minRow->addStretch(1);
    rangeLayout->addLayout(minRow);

    auto* maxRow = new QHBoxLayout();
    maxRow->addWidget(new QLabel(QStringLiteral("最大值:"), m_rangeGroup));
    m_maxSpin = new QSpinBox(m_rangeGroup);
    m_maxSpin->setRange(0, 100);
    m_maxSpin->setValue(80);
    m_maxSpin->setSuffix(QStringLiteral("%"));
    maxRow->addWidget(m_maxSpin);
    auto* setMaxBtn = new QPushButton(QStringLiteral("设置"), m_rangeGroup);
    setMaxBtn->setCursor(Qt::PointingHandCursor);
    connect(setMaxBtn, &QPushButton::clicked, this, &BrightnessWidget::onSetMax);
    maxRow->addWidget(setMaxBtn);
    maxRow->addStretch(1);
    rangeLayout->addLayout(maxRow);

    layout->addWidget(m_rangeGroup);
}

int BrightnessWidget::getBrightness() const
{
    if (m_modeCombo->currentData().toInt() == 1)
        return BRIGHTNESS_AUTO;
    return m_slider->value();
}

int BrightnessWidget::getMin() const
{
    return m_minSpin->value();
}

int BrightnessWidget::getMax() const
{
    return m_maxSpin->value();
}

void BrightnessWidget::setMin(int value)
{
    m_minSpin->setValue(value);
}

void BrightnessWidget::setMax(int value)
{
    m_maxSpin->setValue(value);
}

void BrightnessWidget::applyModeState(bool isAuto)
{
    m_slider->setEnabled(!isAuto);
    m_spin->setEnabled(!isAuto);
    m_setBtn->setEnabled(!isAuto);
    m_rangeGroup->setEnabled(isAuto);
}

void BrightnessWidget::onModeChanged(int index)
{
    const bool isAuto = (index == 1);
    applyModeState(isAuto);
    emit brightnessChanged(isAuto ? BRIGHTNESS_AUTO : m_slider->value());
}

void BrightnessWidget::onSliderChanged(int value)
{
    m_spin->setValue(value);
}

void BrightnessWidget::onSpinChanged(int value)
{
    m_slider->setValue(value);
}

void BrightnessWidget::onSetBrightness()
{
    if (m_modeCombo->currentData().toInt() == 1)
        emit brightnessChanged(BRIGHTNESS_AUTO);
    else
        emit brightnessChanged(m_slider->value());
}

void BrightnessWidget::onSetMin()
{
    emit brightnessMinChanged(m_minSpin->value());
}

void BrightnessWidget::onSetMax()
{
    emit brightnessMaxChanged(m_maxSpin->value());
}

void BrightnessWidget::updateBrightness(int value)
{
    const bool isAuto = (value == BRIGHTNESS_AUTO);
    {
        // Avoid re-emitting brightnessChanged from the mode change.
        QSignalBlocker blocker(m_modeCombo);
        m_modeCombo->setCurrentIndex(isAuto ? 1 : 0);
    }
    applyModeState(isAuto);
    if (!isAuto)
        m_slider->setValue(value);
}
