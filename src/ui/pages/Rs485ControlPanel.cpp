#include "Rs485ControlPanel.h"

#include "protocol/rs485/Rs485Commands.h"
#include "protocol/rs485/Rs485Frame.h"
#include "ui/widgets/BrightnessWidget.h"
#include "ui/widgets/ConnectConfigPanel.h"
#include "ui/widgets/DacScaleWidget.h"
#include "ui/widgets/DisplayWidget.h"
#include "ui/widgets/HexDumpPanel.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>

Rs485ControlPanel::Rs485ControlPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(6);

    // Connection config (serial only).
    m_connectPanel = new ConnectConfigPanel(ProtocolConnectMode::SerialOnly, this, 115200);
    mainLayout->addWidget(m_connectPanel);

    // Device-id change group.
    auto* idGroup = new QGroupBox(QStringLiteral("设备ID修改"), this);
    auto* idLayout = new QHBoxLayout(idGroup);
    idLayout->addWidget(new QLabel(QStringLiteral("当前ID:"), idGroup));
    m_currentIdSpin = new QSpinBox(idGroup);
    m_currentIdSpin->setRange(1, 255);
    m_currentIdSpin->setValue(1);
    idLayout->addWidget(m_currentIdSpin);

    idLayout->addWidget(new QLabel(QStringLiteral("目标ID:"), idGroup));
    m_newIdSpin = new QSpinBox(idGroup);
    m_newIdSpin->setRange(1, 255);
    m_newIdSpin->setValue(2);
    idLayout->addWidget(m_newIdSpin);

    auto* setIdBtn = new QPushButton(QStringLiteral("修改ID"), idGroup);
    setIdBtn->setObjectName(QStringLiteral("warningBtn"));
    setIdBtn->setCursor(Qt::PointingHandCursor);
    connect(setIdBtn, &QPushButton::clicked, this, &Rs485ControlPanel::onSetDeviceId);
    idLayout->addWidget(setIdBtn);
    idLayout->addStretch(1);
    mainLayout->addWidget(idGroup);

    // Three columns: display-state radios, LED visualization, brightness.
    auto* columnsLayout = new QHBoxLayout();
    columnsLayout->setSpacing(12);

    auto* displayGroup = new QGroupBox(QStringLiteral("显示状态"), this);
    displayGroup->setMinimumWidth(240);
    auto* displayLayout = new QVBoxLayout(displayGroup);

    auto* frontLabel = new QLabel(QStringLiteral("正面:"), displayGroup);
    frontLabel->setStyleSheet(QStringLiteral("font-weight: bold;"));
    displayLayout->addWidget(frontLabel);

    m_frontGroup = new QButtonGroup(this);
    auto* frontRow = new QHBoxLayout();
    const struct {
        const char* text;
        int id;
    } frontStates[] = {
        { "关闭", Rs485Commands::FRONT_OFF },
        { "红", Rs485Commands::FRONT_RED },
        { "绿", Rs485Commands::FRONT_GREEN },
        { "转", Rs485Commands::FRONT_TURN },
    };
    for (const auto& s : frontStates) {
        auto* btn = new QRadioButton(QString::fromUtf8(s.text), displayGroup);
        m_frontGroup->addButton(btn, s.id);
        frontRow->addWidget(btn);
    }
    frontRow->addStretch(1);
    displayLayout->addLayout(frontRow);
    if (!m_frontGroup->buttons().isEmpty())
        m_frontGroup->buttons().first()->setChecked(true);

    auto* backLabel = new QLabel(QStringLiteral("背面:"), displayGroup);
    backLabel->setStyleSheet(QStringLiteral("font-weight: bold;"));
    displayLayout->addWidget(backLabel);

    m_backGroup = new QButtonGroup(this);
    auto* backRow = new QHBoxLayout();
    const struct {
        const char* text;
        int id;
    } backStates[] = {
        { "关闭", Rs485Commands::BACK_OFF },
        { "红", Rs485Commands::BACK_RED },
        { "绿", Rs485Commands::BACK_GREEN },
        { "转", Rs485Commands::BACK_TURN },
    };
    for (const auto& s : backStates) {
        auto* btn = new QRadioButton(QString::fromUtf8(s.text), displayGroup);
        m_backGroup->addButton(btn, s.id);
        backRow->addWidget(btn);
    }
    backRow->addStretch(1);
    displayLayout->addLayout(backRow);
    if (!m_backGroup->buttons().isEmpty())
        m_backGroup->buttons().first()->setChecked(true);

    auto* btnRow = new QHBoxLayout();
    auto* sendDisplayBtn = new QPushButton(QStringLiteral("发送显示状态"), displayGroup);
    sendDisplayBtn->setObjectName(QStringLiteral("primaryBtn"));
    sendDisplayBtn->setCursor(Qt::PointingHandCursor);
    connect(sendDisplayBtn, &QPushButton::clicked, this, &Rs485ControlPanel::onSendDisplay);
    btnRow->addWidget(sendDisplayBtn);

    auto* queryDisplayBtn = new QPushButton(QStringLiteral("查询显示状态"), displayGroup);
    queryDisplayBtn->setCursor(Qt::PointingHandCursor);
    connect(queryDisplayBtn, &QPushButton::clicked, this, &Rs485ControlPanel::onQueryDisplay);
    btnRow->addWidget(queryDisplayBtn);
    displayLayout->addLayout(btnRow);
    displayLayout->addStretch(1);
    columnsLayout->addWidget(displayGroup, 1);

    m_displayWidget = new DisplayWidget(this);
    m_displayWidget->setMinimumWidth(240);
    columnsLayout->addWidget(m_displayWidget, 1);

    m_brightnessWidget = new BrightnessWidget(this);
    m_brightnessWidget->setMinimumWidth(280);
    connect(m_brightnessWidget, &BrightnessWidget::brightnessChanged,
            this, &Rs485ControlPanel::onBrightnessChanged);
    connect(m_brightnessWidget, &BrightnessWidget::queryBrightnessRequested,
            this, &Rs485ControlPanel::onQueryBrightness);
    connect(m_brightnessWidget, &BrightnessWidget::brightnessMinChanged,
            this, &Rs485ControlPanel::onBrightnessMinChanged);
    connect(m_brightnessWidget, &BrightnessWidget::brightnessMaxChanged,
            this, &Rs485ControlPanel::onBrightnessMaxChanged);
    columnsLayout->addWidget(m_brightnessWidget, 1);

    mainLayout->addLayout(columnsLayout, 1);

    // Bottom row: baud rate + DAC coefficients.
    auto* bottomRow = new QHBoxLayout();

    auto* baudGroup = new QGroupBox(QStringLiteral("波特率 (2.7, 0x08)"), this);
    auto* baudLayout = new QHBoxLayout(baudGroup);
    baudLayout->addWidget(new QLabel(QStringLiteral("设备波特率:"), baudGroup));
    m_baudCombo = new QComboBox(baudGroup);
    m_baudCombo->addItem(QStringLiteral("9600"), 0);
    m_baudCombo->addItem(QStringLiteral("115200"), 1);
    m_baudCombo->setCurrentIndex(1);
    baudLayout->addWidget(m_baudCombo);
    auto* setBaudBtn = new QPushButton(QStringLiteral("写入设备"), baudGroup);
    setBaudBtn->setObjectName(QStringLiteral("warningBtn"));
    setBaudBtn->setCursor(Qt::PointingHandCursor);
    connect(setBaudBtn, &QPushButton::clicked, this, &Rs485ControlPanel::onSetBaudRate);
    baudLayout->addWidget(setBaudBtn);
    baudLayout->addWidget(new QLabel(QStringLiteral("提示: 改波特率后请切换上位机串口速率"), baudGroup));
    bottomRow->addWidget(baudGroup);

    m_dacScaleWidget = new DacScaleWidget(this);
    connect(m_dacScaleWidget, &DacScaleWidget::dacRedChanged,
            this, &Rs485ControlPanel::onDacRedChanged);
    connect(m_dacScaleWidget, &DacScaleWidget::dacGreenChanged,
            this, &Rs485ControlPanel::onDacGreenChanged);
    bottomRow->addWidget(m_dacScaleWidget, 1);

    mainLayout->addLayout(bottomRow);

    // Hex dump panel.
    m_hexPanel = new HexDumpPanel(this);
    connect(m_hexPanel, &HexDumpPanel::sendHex, this, &Rs485ControlPanel::sendFrame);
    mainLayout->addWidget(m_hexPanel);
}

void Rs485ControlPanel::onSetDeviceId()
{
    const int currentId = m_currentIdSpin->value();
    const int newId = m_newIdSpin->value();
    if (currentId == newId) {
        emit logMessage(QStringLiteral("当前ID与目标ID相同，无需修改"), QStringLiteral("WARN"));
        return;
    }
    const QByteArray frame =
        Rs485Commands::buildDeviceIdFrame(static_cast<quint8>(currentId),
                                          static_cast<quint8>(newId));
    emit sendFrame(frame);
    emit logMessage(QStringLiteral("修改设备ID: %1 (ID=%2 → %3)")
                        .arg(Rs485Frame::frameToHex(frame))
                        .arg(currentId)
                        .arg(newId),
                    QStringLiteral("CMD"));
}

void Rs485ControlPanel::onSendDisplay()
{
    const quint8 front = static_cast<quint8>(m_frontGroup->checkedId());
    const quint8 back = static_cast<quint8>(m_backGroup->checkedId());
    const QByteArray frame = Rs485Commands::buildDisplayStateFrame(
        static_cast<quint8>(m_currentIdSpin->value()), front, back);
    emit sendFrame(frame);
    emit logMessage(QStringLiteral("发送显示状态: %1 (%2)")
                        .arg(Rs485Frame::frameToHex(frame),
                             Rs485Commands::describeDisplayState(
                                 Rs485Commands::combineDisplay(front, back))),
                    QStringLiteral("CMD"));
}

void Rs485ControlPanel::onQueryDisplay()
{
    const QByteArray frame =
        Rs485Commands::buildQueryDisplayStateFrame(static_cast<quint8>(m_currentIdSpin->value()));
    emit sendFrame(frame);
    emit logMessage(QStringLiteral("查询显示状态: %1").arg(Rs485Frame::frameToHex(frame)),
                    QStringLiteral("CMD"));
}

void Rs485ControlPanel::onBrightnessChanged(int value)
{
    const QByteArray frame = Rs485Commands::buildBrightnessFrame(
        static_cast<quint8>(m_currentIdSpin->value()), static_cast<quint8>(value));
    emit sendFrame(frame);
    emit logMessage(QStringLiteral("设置亮度: %1 (%2)")
                        .arg(Rs485Frame::frameToHex(frame),
                             Rs485Commands::describeBrightness(static_cast<quint8>(value))),
                    QStringLiteral("CMD"));
}

void Rs485ControlPanel::onQueryBrightness()
{
    const QByteArray frame =
        Rs485Commands::buildQueryBrightnessFrame(static_cast<quint8>(m_currentIdSpin->value()));
    emit sendFrame(frame);
    emit logMessage(QStringLiteral("查询亮度: %1").arg(Rs485Frame::frameToHex(frame)),
                    QStringLiteral("CMD"));
}

void Rs485ControlPanel::onBrightnessMinChanged(int value)
{
    const QByteArray frame = Rs485Commands::buildBrightnessMinFrame(
        static_cast<quint8>(m_currentIdSpin->value()), static_cast<quint8>(value));
    emit sendFrame(frame);
    emit logMessage(QStringLiteral("设置亮度最小值: %1 (%2%)")
                        .arg(Rs485Frame::frameToHex(frame))
                        .arg(value),
                    QStringLiteral("CMD"));
}

void Rs485ControlPanel::onBrightnessMaxChanged(int value)
{
    const QByteArray frame = Rs485Commands::buildBrightnessMaxFrame(
        static_cast<quint8>(m_currentIdSpin->value()), static_cast<quint8>(value));
    emit sendFrame(frame);
    emit logMessage(QStringLiteral("设置亮度最大值: %1 (%2%)")
                        .arg(Rs485Frame::frameToHex(frame))
                        .arg(value),
                    QStringLiteral("CMD"));
}

void Rs485ControlPanel::onSetBaudRate()
{
    const int baudCode = m_baudCombo->currentData().toInt();
    const int currentId = m_currentIdSpin->value();
    const QByteArray frame = Rs485Commands::buildBaudRateFrame(
        static_cast<quint8>(currentId), static_cast<quint8>(baudCode));
    emit sendFrame(frame);
    const QString baudRate = (baudCode == 0) ? QStringLiteral("9600")
                                             : QStringLiteral("115200");
    emit logMessage(QStringLiteral("修改波特率: %1 (ID=%2, 波特率=%3)")
                        .arg(Rs485Frame::frameToHex(frame))
                        .arg(currentId)
                        .arg(baudRate),
                    QStringLiteral("CMD"));
}

void Rs485ControlPanel::onDacRedChanged(int value)
{
    const QByteArray frame = Rs485Commands::buildDacScaleRedFrame(
        static_cast<quint8>(m_currentIdSpin->value()), static_cast<quint8>(value));
    emit sendFrame(frame);
    emit logMessage(QStringLiteral("红系数 0x09: %1 (系数=%2)")
                        .arg(Rs485Frame::frameToHex(frame))
                        .arg(value),
                    QStringLiteral("CMD"));
}

void Rs485ControlPanel::onDacGreenChanged(int value)
{
    const QByteArray frame = Rs485Commands::buildDacScaleGreenFrame(
        static_cast<quint8>(m_currentIdSpin->value()), static_cast<quint8>(value));
    emit sendFrame(frame);
    emit logMessage(QStringLiteral("绿系数 0x0A: %1 (系数=%2)")
                        .arg(Rs485Frame::frameToHex(frame))
                        .arg(value),
                    QStringLiteral("CMD"));
}

void Rs485ControlPanel::handleResponse(const QByteArray& data)
{
    quint8 deviceId = 0, cmd = 0, dataByte = 0;
    bool valid = false;
    if (!Rs485Frame::parseFrame(data, &deviceId, &cmd, &dataByte, &valid))
        return;
    if (!valid)
        return;
    if (cmd < 0x80)
        return; // echo, not a response

    switch (cmd) {
    case Rs485Commands::RESP_SET_DISPLAY_STATE:
    case Rs485Commands::RESP_QUERY_DISPLAY_STATE:
        m_displayWidget->updateState(dataByte & 0xF0, dataByte & 0x0F);
        break;
    case Rs485Commands::RESP_SET_BRIGHTNESS:
    case Rs485Commands::RESP_QUERY_BRIGHTNESS:
        m_brightnessWidget->updateBrightness(dataByte);
        break;
    case Rs485Commands::RESP_SET_BRIGHTNESS_MIN:
        m_brightnessWidget->setMin(dataByte);
        break;
    case Rs485Commands::RESP_SET_BRIGHTNESS_MAX:
        m_brightnessWidget->setMax(dataByte);
        break;
    case Rs485Commands::RESP_SET_BAUD_RATE:
        for (int i = 0; i < m_baudCombo->count(); ++i) {
            if (m_baudCombo->itemData(i).toInt() == dataByte) {
                m_baudCombo->setCurrentIndex(i);
                break;
            }
        }
        break;
    case Rs485Commands::RESP_SET_DAC_SCALE_RED:
        m_dacScaleWidget->updateRed(dataByte);
        break;
    case Rs485Commands::RESP_SET_DAC_SCALE_GREEN:
        m_dacScaleWidget->updateGreen(dataByte);
        break;
    default:
        break;
    }
}
