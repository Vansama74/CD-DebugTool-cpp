#include "IapUpgradePanel.h"

#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

#include "config/ConfigManager.h"

IapUpgradePanel::IapUpgradePanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(12);

    // Firmware file group.
    auto* fwGroup = new QGroupBox(QStringLiteral("固件文件"), this);
    auto* fwLayout = new QVBoxLayout(fwGroup);

    auto* pathRow = new QHBoxLayout();
    m_pathEdit = new QLineEdit(fwGroup);
    m_pathEdit->setReadOnly(true);
    m_pathEdit->setPlaceholderText(QStringLiteral("选择固件文件..."));

    // 用上次保存的固件路径预填。
    m_firmwarePath =
        ConfigManager::getString(ConfigManager::load(), "last_firmware_path", QString());
    if (!m_firmwarePath.isEmpty())
        m_pathEdit->setText(m_firmwarePath);
    pathRow->addWidget(m_pathEdit);

    auto* browseBtn = new QPushButton(QStringLiteral("选择文件"), fwGroup);
    browseBtn->setMinimumWidth(100);
    browseBtn->setCursor(Qt::PointingHandCursor);
    connect(browseBtn, &QPushButton::clicked, this, &IapUpgradePanel::browseFile);
    pathRow->addWidget(browseBtn);
    fwLayout->addLayout(pathRow);

    auto* infoRow = new QHBoxLayout();
    m_sizeLabel = new QLabel(QStringLiteral("大小: --"), fwGroup);
    m_sizeLabel->setObjectName(QStringLiteral("subtitleLabel"));
    infoRow->addWidget(m_sizeLabel);

    m_crcLabel = new QLabel(QStringLiteral("CRC: --"), fwGroup);
    m_crcLabel->setObjectName(QStringLiteral("subtitleLabel"));
    infoRow->addWidget(m_crcLabel);

    m_packetsLabel = new QLabel(QStringLiteral("包数: --"), fwGroup);
    m_packetsLabel->setObjectName(QStringLiteral("subtitleLabel"));
    infoRow->addWidget(m_packetsLabel);
    infoRow->addStretch(1);
    fwLayout->addLayout(infoRow);

    layout->addWidget(fwGroup);

    // Progress group.
    auto* progressGroup = new QGroupBox(QStringLiteral("升级进度"), this);
    auto* progressLayout = new QVBoxLayout(progressGroup);

    m_progress = new QProgressBar(progressGroup);
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progress->setFormat(QStringLiteral("%p%"));
    progressLayout->addWidget(m_progress);

    m_statusLabel = new QLabel(QStringLiteral("就绪"), progressGroup);
    m_statusLabel->setObjectName(QStringLiteral("statusLabel"));
    m_statusLabel->setAlignment(Qt::AlignCenter);
    progressLayout->addWidget(m_statusLabel);

    m_pktLabel = new QLabel(QStringLiteral("--"), progressGroup);
    m_pktLabel->setObjectName(QStringLiteral("subtitleLabel"));
    m_pktLabel->setAlignment(Qt::AlignCenter);
    progressLayout->addWidget(m_pktLabel);

    layout->addWidget(progressGroup);

    // Control group.
    auto* ctrlGroup = new QGroupBox(QStringLiteral("操作"), this);
    auto* ctrlLayout = new QVBoxLayout(ctrlGroup);

    auto* mainRow = new QHBoxLayout();
    m_startBtn = new QPushButton(QStringLiteral("开始升级"), ctrlGroup);
    m_startBtn->setObjectName(QStringLiteral("primaryBtn"));
    m_startBtn->setCursor(Qt::PointingHandCursor);
    connect(m_startBtn, &QPushButton::clicked, this, &IapUpgradePanel::onStart);
    mainRow->addWidget(m_startBtn);

    m_cancelBtn = new QPushButton(QStringLiteral("取消"), ctrlGroup);
    m_cancelBtn->setObjectName(QStringLiteral("dangerBtn"));
    m_cancelBtn->setCursor(Qt::PointingHandCursor);
    connect(m_cancelBtn, &QPushButton::clicked, this, &IapUpgradePanel::onCancel);
    mainRow->addWidget(m_cancelBtn);
    ctrlLayout->addLayout(mainRow);

    auto* auxRow = new QHBoxLayout();
    m_pauseBtn = new QPushButton(QStringLiteral("暂停"), ctrlGroup);
    m_pauseBtn->setCursor(Qt::PointingHandCursor);
    connect(m_pauseBtn, &QPushButton::clicked, this, &IapUpgradePanel::onPauseResume);
    auxRow->addWidget(m_pauseBtn);

    m_rebootBtn = new QPushButton(QStringLiteral("重启设备"), ctrlGroup);
    m_rebootBtn->setCursor(Qt::PointingHandCursor);
    connect(m_rebootBtn, &QPushButton::clicked, this, &IapUpgradePanel::rebootRequested);
    auxRow->addWidget(m_rebootBtn);

    m_recoveryBtn = new QPushButton(QStringLiteral("恢复模式"), ctrlGroup);
    m_recoveryBtn->setCursor(Qt::PointingHandCursor);
    connect(m_recoveryBtn, &QPushButton::clicked, this, &IapUpgradePanel::recoveryRequested);
    auxRow->addWidget(m_recoveryBtn);

    ctrlLayout->addLayout(auxRow);
    layout->addWidget(ctrlGroup);

    layout->addStretch(1);
}

void IapUpgradePanel::browseFile()
{
    const QString filepath = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择固件文件"), m_firmwarePath,
        QStringLiteral("固件文件 (*.bin *.hex *.img);;所有文件 (*)"));
    if (filepath.isEmpty())
        return;
    m_firmwarePath = filepath;
    m_pathEdit->setText(filepath);

    // 选固件成功后持久化路径，下次启动预填。
    QJsonObject cfg = ConfigManager::load();
    cfg.insert(QStringLiteral("last_firmware_path"), filepath);
    ConfigManager::save(cfg);

    emit firmwareSelected(filepath);
}

void IapUpgradePanel::onStart()
{
    if (m_firmwarePath.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先选择固件文件"));
        return;
    }
    emit startUpgrade();
}

void IapUpgradePanel::onCancel()
{
    if (!m_upgrading)
        return;
    emit cancelUpgrade();
}

void IapUpgradePanel::onPauseResume()
{
    if (!m_upgrading)
        return;
    if (m_pauseBtn->text() == QStringLiteral("暂停")) {
        m_pauseBtn->setText(QStringLiteral("继续"));
        emit pauseUpgrade();
    } else {
        m_pauseBtn->setText(QStringLiteral("暂停"));
        emit resumeUpgrade();
    }
}

void IapUpgradePanel::updateFirmwareInfo(const QJsonObject& info)
{
    const qint64 size = static_cast<qint64>(info.value(QStringLiteral("size")).toDouble());
    if (size >= 1024 * 1024)
        m_sizeLabel->setText(
            QStringLiteral("大小: %1 MB").arg(size / (1024.0 * 1024.0), 0, 'f', 2));
    else if (size >= 1024)
        m_sizeLabel->setText(QStringLiteral("大小: %1 KB").arg(size / 1024.0, 0, 'f', 1));
    else
        m_sizeLabel->setText(QStringLiteral("大小: %1 B").arg(size));

    const quint32 crc = static_cast<quint32>(info.value(QStringLiteral("crc")).toDouble());
    m_crcLabel->setText(QStringLiteral("CRC: 0x")
                        + QString::number(crc, 16).toUpper().rightJustified(8, QLatin1Char('0')));

    const int packets = static_cast<int>(info.value(QStringLiteral("totalPackets")).toDouble());
    m_packetsLabel->setText(QStringLiteral("包数: %1").arg(packets));
}

void IapUpgradePanel::setUpgrading(bool upgrading)
{
    m_upgrading = upgrading;
    m_startBtn->setEnabled(!upgrading);
    m_cancelBtn->setEnabled(upgrading);
    if (!upgrading)
        m_pauseBtn->setText(QStringLiteral("暂停"));
}

void IapUpgradePanel::updateProgress(double percent, int current, int total)
{
    m_progress->setValue(static_cast<int>(percent));
    m_pktLabel->setText(QStringLiteral("传输中: 包 %1/%2").arg(current).arg(total));
}

void IapUpgradePanel::updateStatus(const QString& text)
{
    m_statusLabel->setText(text);
}
