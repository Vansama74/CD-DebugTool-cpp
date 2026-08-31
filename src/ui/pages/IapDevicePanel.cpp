#include "IapDevicePanel.h"

#include "core/Device.h"

#include <QBrush>
#include <QColor>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSize>
#include <QVBoxLayout>

IapDevicePanel::IapDevicePanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto* group = new QGroupBox(QStringLiteral("设备列表"), this);
    auto* gl = new QVBoxLayout(group);

    m_list = new QListWidget(group);
    m_list->setIconSize(QSize(12, 12));
    connect(m_list, &QListWidget::currentItemChanged, this,
            [this](QListWidgetItem* cur, QListWidgetItem*) {
                if (cur) {
                    const QString id = cur->data(Qt::UserRole).toString();
                    if (!id.isEmpty())
                        emit deviceSelected(id);
                }
            });
    connect(m_list, &QListWidget::itemChanged, this, &IapDevicePanel::onItemChanged);
    gl->addWidget(m_list);

    auto* row = new QHBoxLayout();
    m_scanBtn = new QPushButton(QStringLiteral("扫描设备"), group);
    m_scanBtn->setObjectName(QStringLiteral("primaryBtn"));
    m_scanBtn->setCursor(Qt::PointingHandCursor);
    connect(m_scanBtn, &QPushButton::clicked, this, &IapDevicePanel::scanRequested);
    row->addWidget(m_scanBtn);

    m_selectAllBtn = new QPushButton(QStringLiteral("全选"), group);
    m_selectAllBtn->setCursor(Qt::PointingHandCursor);
    connect(m_selectAllBtn, &QPushButton::clicked, this, &IapDevicePanel::selectAllRequested);
    row->addWidget(m_selectAllBtn);
    gl->addLayout(row);

    layout->addWidget(group);

    // —— 设备配置区块：当前设备网络配置展示 + setip 下发 + 固件状态查询 ——
    auto* cfgGroup = new QGroupBox(QStringLiteral("设备配置"), this);
    auto* cg = new QVBoxLayout(cfgGroup);
    cg->setSpacing(6);

    m_curInfoLabel = new QLabel(QStringLiteral("当前: 未选择设备"), cfgGroup);
    m_curInfoLabel->setObjectName(QStringLiteral("subtitleLabel"));
    m_curInfoLabel->setWordWrap(true);
    cg->addWidget(m_curInfoLabel);

    auto* form = new QFormLayout();
    m_ipEdit = new QLineEdit(cfgGroup);
    m_ipEdit->setPlaceholderText(QStringLiteral("如 192.168.114.200"));
    form->addRow(QStringLiteral("IP:"), m_ipEdit);
    m_maskEdit = new QLineEdit(cfgGroup);
    m_maskEdit->setPlaceholderText(QStringLiteral("如 255.255.255.0"));
    form->addRow(QStringLiteral("掩码:"), m_maskEdit);
    m_gwEdit = new QLineEdit(cfgGroup);
    m_gwEdit->setPlaceholderText(QStringLiteral("如 192.168.114.1"));
    form->addRow(QStringLiteral("网关:"), m_gwEdit);
    m_portEdit = new QLineEdit(cfgGroup);
    m_portEdit->setPlaceholderText(QStringLiteral("1~65535"));
    form->addRow(QStringLiteral("端口:"), m_portEdit);
    cg->addLayout(form);

    auto* cfgRow = new QHBoxLayout();
    auto* applyBtn = new QPushButton(QStringLiteral("下发配置"), cfgGroup);
    applyBtn->setObjectName(QStringLiteral("primaryBtn"));
    applyBtn->setCursor(Qt::PointingHandCursor);
    connect(applyBtn, &QPushButton::clicked, this, [this]() {
        emit configApplyRequested(m_ipEdit->text().trimmed(), m_maskEdit->text().trimmed(),
                                  m_gwEdit->text().trimmed(), m_portEdit->text().trimmed());
    });
    cfgRow->addWidget(applyBtn);

    auto* queryBtn = new QPushButton(QStringLiteral("获取固件状态"), cfgGroup);
    queryBtn->setCursor(Qt::PointingHandCursor);
    connect(queryBtn, &QPushButton::clicked, this, &IapDevicePanel::statusQueryRequested);
    cfgRow->addWidget(queryBtn);
    cg->addLayout(cfgRow);

    m_fwStatusLabel = new QLabel(QStringLiteral("固件状态: --"), cfgGroup);
    m_fwStatusLabel->setObjectName(QStringLiteral("subtitleLabel"));
    m_fwStatusLabel->setWordWrap(true);
    cg->addWidget(m_fwStatusLabel);

    layout->addWidget(cfgGroup);
    layout->addStretch(1);
}

void IapDevicePanel::addDevice(Device* device)
{
    auto* item = new QListWidgetItem(m_list);
    item->setData(Qt::UserRole, device->deviceId);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    updateItem(item, device);
}

void IapDevicePanel::updateDevice(Device* device)
{
    for (int i = 0; i < m_list->count(); ++i) {
        QListWidgetItem* item = m_list->item(i);
        if (item->data(Qt::UserRole).toString() == device->deviceId) {
            updateItem(item, device);
            return;
        }
    }
}

void IapDevicePanel::removeDevice(const QString& deviceId)
{
    for (int i = 0; i < m_list->count(); ++i) {
        if (m_list->item(i)->data(Qt::UserRole).toString() == deviceId) {
            delete m_list->takeItem(i);
            return;
        }
    }
}

void IapDevicePanel::clearDevices()
{
    m_list->clear();
}

void IapDevicePanel::showDeviceConfig(Device* device)
{
    if (!device) {
        m_curInfoLabel->setText(QStringLiteral("当前: 未选择设备"));
        m_fwStatusLabel->setText(QStringLiteral("固件状态: --"));
        return;
    }
    m_curInfoLabel->setText(
        QStringLiteral("当前: IP %1  掩码 %2  网关 %3  端口 %4")
            .arg(device->ip, device->mask, device->gateway)
            .arg(device->appPort));
    // 预填输入框（用户可编辑后下发）。
    m_ipEdit->setText(device->ip);
    m_maskEdit->setText(device->mask);
    m_gwEdit->setText(device->gateway);
    m_portEdit->setText(QString::number(device->appPort));
    updateDeviceInfo(device);
}

void IapDevicePanel::updateDeviceInfo(Device* device)
{
    if (!device)
        return;
    m_curInfoLabel->setText(
        QStringLiteral("当前: IP %1  掩码 %2  网关 %3  端口 %4")
            .arg(device->ip, device->mask, device->gateway)
            .arg(device->appPort));
    if (device->fwVersion.isEmpty()) {
        m_fwStatusLabel->setText(QStringLiteral("固件状态: --（点击「获取固件状态」查询）"));
        return;
    }
    m_fwStatusLabel->setText(
        QStringLiteral("固件状态: %1 | 大小 %2 B | CRC 0x%3 | %4")
            .arg(device->fwVersion)
            .arg(device->fwSize.load())
            .arg(QString::number(device->fwCrc.load(), 16).toUpper()
                     .rightJustified(8, QLatin1Char('0')))
            .arg(upgradeStateLabel(device->upgradeState.load())));
}

void IapDevicePanel::onItemChanged(QListWidgetItem* item)
{
    if (!item)
        return;
    const QString id = item->data(Qt::UserRole).toString();
    if (id.isEmpty())
        return;
    emit selectionChanged(id, item->checkState() == Qt::Checked);
}

void IapDevicePanel::updateItem(QListWidgetItem* item, Device* device)
{
    const DeviceStatus status = device->status.load();
    const QString dot = device->isOnline() ? QStringLiteral("●") : QStringLiteral("○");

    QString sub = QStringLiteral("   %1 %2").arg(dot, deviceStatusLabel(status));
    if (status == DeviceStatus::Transferring || status == DeviceStatus::Verifying)
        sub += QStringLiteral(" [%1%]").arg(device->progress);

    {
        // Programmatic updates must not fire itemChanged (avoids feedback loop).
        const QSignalBlocker blocker(m_list);
        item->setText(device->displayName() + QStringLiteral("\n") + sub);
        item->setCheckState(device->selected ? Qt::Checked : Qt::Unchecked);
        item->setForeground(QBrush(QColor(QStringLiteral("#CDD6F4"))));
    }
}