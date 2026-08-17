#include "IapDevicePanel.h"

#include "core/Device.h"

#include <QBrush>
#include <QColor>
#include <QGroupBox>
#include <QHBoxLayout>
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
