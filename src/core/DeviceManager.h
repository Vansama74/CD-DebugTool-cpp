#pragma once
#include <QHash>
#include <QObject>
#include <QTimer>
#include <QVector>

#include "Device.h"
#include "protocol/iap/IapFrame.h"

// Tracks discovered devices and their upgrade lifecycle state. A 30s timeout
// (checked on a 5s tick) marks an Online device Offline when no frames arrive.
class DeviceManager : public QObject {
    Q_OBJECT
public:
    explicit DeviceManager(QObject* parent = nullptr);

    QVector<Device*> devices();
    Device* getDevice(const QString& id);
    QVector<Device*> getOnlineDevices();
    QVector<Device*> getSelectedDevices();

    void addOrUpdateDevice(const Device& device);
    void handleFrame(const IapFrame::ParsedFrame& parsed, const QString& srcId,
                     TransportType transport = TransportType::Udp);
    void markOffline(const QString& id);
    void removeDevice(const QString& id);
    void clearAll();

signals:
    void deviceAdded(const QString& id);
    void deviceUpdated(const QString& id);
    void deviceRemoved(const QString& id);
    void logMessage(const QString& msg);

private slots:
    void checkTimeouts();

private:
    qint64 nowMs() const;

    QHash<QString, Device> m_devices;
    QHash<QString, qint64> m_lastSeen;
    QTimer m_timer;
};
