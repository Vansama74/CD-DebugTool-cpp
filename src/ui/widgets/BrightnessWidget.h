#pragma once
#include <QWidget>

class QComboBox;
class QGroupBox;
class QPushButton;
class QSlider;
class QSpinBox;

// Brightness control: manual/auto mode, 0-100 slider+spin, min/max range config.
class BrightnessWidget : public QWidget {
    Q_OBJECT
public:
    explicit BrightnessWidget(QWidget* parent = nullptr);

    int getBrightness() const; // 0..100, or 0xFF for auto
    int getMin() const;
    int getMax() const;

    void setMin(int value);
    void setMax(int value);
    void updateBrightness(int value); // 0..100, or 0xFF for auto

signals:
    void brightnessChanged(int value); // 0..100 or 0xFF for auto
    void queryBrightnessRequested();
    void brightnessMinChanged(int value);
    void brightnessMaxChanged(int value);

private slots:
    void onModeChanged(int index);
    void onSliderChanged(int value);
    void onSpinChanged(int value);
    void onSetBrightness();
    void onSetMin();
    void onSetMax();

private:
    void applyModeState(bool isAuto);

    QComboBox* m_modeCombo = nullptr;
    QSlider* m_slider = nullptr;
    QSpinBox* m_spin = nullptr;
    QPushButton* m_setBtn = nullptr;
    QPushButton* m_queryBtn = nullptr;
    QGroupBox* m_rangeGroup = nullptr;
    QSpinBox* m_minSpin = nullptr;
    QSpinBox* m_maxSpin = nullptr;
};
