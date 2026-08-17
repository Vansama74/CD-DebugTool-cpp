#pragma once
#include <QWidget>

class QSpinBox;

// Red/green DAC brightness coefficients (1~40), written to flash.
class DacScaleWidget : public QWidget {
    Q_OBJECT
public:
    explicit DacScaleWidget(QWidget* parent = nullptr);

    void updateRed(int value);
    void updateGreen(int value);

signals:
    void dacRedChanged(int value);
    void dacGreenChanged(int value);

private slots:
    void onSetRed();
    void onSetGreen();
    void onApplyBoth();

private:
    QSpinBox* m_redSpin = nullptr;
    QSpinBox* m_greenSpin = nullptr;
};
