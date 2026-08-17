#pragma once
#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QStackedWidget;

enum class ProtocolConnectMode {
    SerialOnly,
    UdpOnly,
    UdpAndSerial,
};

// ConnectConfigPanel: reusable connection-config widget shared by protocol pages.
class ConnectConfigPanel : public QWidget {
    Q_OBJECT
public:
    explicit ConnectConfigPanel(ProtocolConnectMode mode, QWidget* parent = nullptr,
                                int defaultBaud = 115200);

    QString getSerialPort() const;
    int getBaudRate() const;
    QString getSelectedNicIp() const;
    int getListenPort() const;
    int getDevicePort() const;
    bool isSerialOpen() const;
    void setSerialOpenState(bool open);

signals:
    void portOpened(const QString& port, int baud);
    void portClosed();
    void transportChanged(int idx);

public slots:
    // transport 发出 disconnected 时回写 UI 状态（关闭串口视觉态）。
    void onTransportDisconnected();

private slots:
    void refreshSerialPorts();
    void refreshNetworkInterfaces();
    void onTransportChanged(int idx);
    void onNicChanged();
    void togglePort();

private:
    QWidget* buildUdpConfig();
    QWidget* buildSerialConfig();
    void applyOpenState(bool open);
    // 用保存的串口/波特率预选下拉框（构造末尾调用）。
    void applySavedSerialConfig();

    ProtocolConnectMode m_mode;
    int m_defaultBaud = 115200;
    bool m_serialOpen = false;

    QComboBox* m_transportCombo = nullptr;
    QStackedWidget* m_transportStack = nullptr;

    QComboBox* m_nicCombo = nullptr;
    QLabel* m_localIpLabel = nullptr;
    QSpinBox* m_listenSpin = nullptr;
    QSpinBox* m_deviceSpin = nullptr;

    QComboBox* m_serialCombo = nullptr;
    QComboBox* m_baudCombo = nullptr;
    QPushButton* m_refreshBtn = nullptr;
    QPushButton* m_openBtn = nullptr;
    QLabel* m_statusLabel = nullptr;
};
