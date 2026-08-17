#pragma once
#include <QWidget>

class QButtonGroup;
class QComboBox;
class QPushButton;
class QSpinBox;
class ConnectConfigPanel;
class BrightnessWidget;
class DacScaleWidget;
class DisplayWidget;
class HexDumpPanel;

// RS485 debug control panel: connection config, display state, brightness,
// DAC coefficients, baud rate, device-id change and a hex dump panel.
class Rs485ControlPanel : public QWidget {
    Q_OBJECT
public:
    explicit Rs485ControlPanel(QWidget* parent = nullptr);

    ConnectConfigPanel* connectPanel() const { return m_connectPanel; }
    HexDumpPanel* hexPanel() const { return m_hexPanel; }

    void handleResponse(const QByteArray& data);

signals:
    void sendFrame(const QByteArray& data);
    void logMessage(const QString& message, const QString& level);

private slots:
    void onSetDeviceId();
    void onSendDisplay();
    void onQueryDisplay();
    void onBrightnessChanged(int value);
    void onQueryBrightness();
    void onBrightnessMinChanged(int value);
    void onBrightnessMaxChanged(int value);
    void onSetBaudRate();
    void onDacRedChanged(int value);
    void onDacGreenChanged(int value);

private:
    ConnectConfigPanel* m_connectPanel = nullptr;
    DisplayWidget* m_displayWidget = nullptr;
    BrightnessWidget* m_brightnessWidget = nullptr;
    DacScaleWidget* m_dacScaleWidget = nullptr;
    HexDumpPanel* m_hexPanel = nullptr;

    QSpinBox* m_currentIdSpin = nullptr;
    QSpinBox* m_newIdSpin = nullptr;

    QButtonGroup* m_frontGroup = nullptr;
    QButtonGroup* m_backGroup = nullptr;

    QComboBox* m_baudCombo = nullptr;
};
