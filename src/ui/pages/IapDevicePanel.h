#pragma once
#include <QWidget>

class QListWidget;
class QListWidgetItem;
class QPushButton;
struct Device;

// Left panel: device list (checkbox + display name + status dot + progress).
// The deviceId is stored in each item's Qt::UserRole.
class IapDevicePanel : public QWidget {
    Q_OBJECT
public:
    explicit IapDevicePanel(QWidget* parent = nullptr);

    void addDevice(Device* device);
    void updateDevice(Device* device);
    void removeDevice(const QString& deviceId);
    void clearDevices();

signals:
    void scanRequested();
    void selectAllRequested();
    void deviceSelected(const QString& deviceId);
    void selectionChanged(const QString& deviceId, bool selected);

private slots:
    void onItemChanged(QListWidgetItem* item);

private:
    void updateItem(QListWidgetItem* item, Device* device);

    QListWidget* m_list = nullptr;
    QPushButton* m_scanBtn = nullptr;
    QPushButton* m_selectAllBtn = nullptr;
};
