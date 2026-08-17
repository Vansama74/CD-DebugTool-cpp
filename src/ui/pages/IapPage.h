#pragma once
#include "core/IProtocolPage.h"
#include "core/Device.h"
#include "core/UpgradeEngine.h"
#include "protocol/iap/IapFrame.h"

#include <QByteArray>

class ConnectConfigPanel;
class IapDevicePanel;
class IapUpgradePanel;
class DeviceManager;
class SerialTransport;
class UdpTransport;

// IAP remote-upgrade page: wires ConnectConfigPanel (UdpAndSerial) -> transports
// -> DeviceManager -> UpgradeEngine -> device/upgrade panels + shared log.
class IapPage : public IProtocolPage {
    Q_OBJECT
public:
    explicit IapPage(QWidget* parent = nullptr);
    ~IapPage() override;

    QString key() const override;
    QString fullName() const override;
    void activate() override;
    void deactivate() override;

private slots:
    void onSerialOpened(const QString& port, int baud);
    void onSerialClosed();
    void onTransportChanged(int idx);
    void onScan();
    void onSelectAll();
    void onSelectionChanged(const QString& id, bool selected);
    void onDeviceAdded(const QString& id);
    void onDeviceUpdated(const QString& id);
    void onDeviceRemoved(const QString& id);
    void onFirmwareSelected(const QString& path);
    void onStartUpgrade();
    void onCancelUpgrade();
    void onReboot();
    void onRecovery();
    void onFrameReceived(const QByteArray& data, const QString& srcIp, quint16 srcPort);
    void onBytesReceived(const QByteArray& data);
    void onEngineProgress(const QString& id, double percent, int current, int total);
    void onEngineStatus(const QString& id, DeviceStatus status);
    void onEngineState(EngineState state);
    void onTransportError(const QString& msg);

private:
    void bindUdp();
    // 线程安全发送：transport / defaultPort 均为入口处快照，worker 线程内
    // 不读取任何 QWidget 成员。
    void sendToDevice(const QByteArray& data, Device* device, TransportType transport,
                      quint16 defaultPort);
    void appendLog(const QString& msg, const QString& level = QStringLiteral("INFO"));
    void handleParsedFrame(const IapFrame::ParsedFrame& parsed, const QString& srcId,
                           TransportType transport);

    ConnectConfigPanel* m_connect = nullptr;
    IapDevicePanel* m_devicePanel = nullptr;
    IapUpgradePanel* m_upgradePanel = nullptr;
    DeviceManager* m_deviceMgr = nullptr;
    UpgradeEngine* m_engine = nullptr;
    SerialTransport* m_serial = nullptr;
    UdpTransport* m_udp = nullptr;
    QByteArray m_rxBuffer;
    TransportType m_transport = TransportType::Udp;
};
