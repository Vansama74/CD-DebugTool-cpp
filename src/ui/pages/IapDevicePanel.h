#pragma once
#include <QWidget>

class QListWidget;
class QListWidgetItem;
class QPushButton;
class QLabel;
class QLineEdit;
struct Device;

// Left panel: device list (checkbox + display name + status dot + progress)
// + 「设备配置」section（当前设备 IP/掩码/网关/端口展示、setip 下发输入、
// 固件状态查询与展示）。The deviceId is stored in each item's Qt::UserRole.
class IapDevicePanel : public QWidget {
    Q_OBJECT
public:
    explicit IapDevicePanel(QWidget* parent = nullptr);

    void addDevice(Device* device);
    void updateDevice(Device* device);
    void removeDevice(const QString& deviceId);
    void clearDevices();
    // 选中设备切换：展示当前配置并预填输入框（用户可编辑）。
    void showDeviceConfig(Device* device);
    // 设备信息刷新（4B01/4B03 应答更新）：只刷新展示标签，不清用户输入。
    void updateDeviceInfo(Device* device);

signals:
    void scanRequested();
    void selectAllRequested();
    void deviceSelected(const QString& deviceId);
    void selectionChanged(const QString& deviceId, bool selected);
    // 下发配置：ip/mask/gateway/portText 为用户输入原文，IapPage 校验。
    void configApplyRequested(const QString& ip, const QString& mask,
                              const QString& gateway, const QString& portText);
    // 查询当前设备固件状态（4B03）。
    void statusQueryRequested();

private slots:
    void onItemChanged(QListWidgetItem* item);

private:
    void updateItem(QListWidgetItem* item, Device* device);

    QListWidget* m_list = nullptr;
    QPushButton* m_scanBtn = nullptr;
    QPushButton* m_selectAllBtn = nullptr;

    QLabel* m_curInfoLabel = nullptr;
    QLineEdit* m_ipEdit = nullptr;
    QLineEdit* m_maskEdit = nullptr;
    QLineEdit* m_gwEdit = nullptr;
    QLineEdit* m_portEdit = nullptr;
    QLabel* m_fwStatusLabel = nullptr;
};