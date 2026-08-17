#include "SerialProtocolPage.h"

#include "transport/SerialTransport.h"
#include "ui/widgets/ConnectConfigPanel.h"
#include "ui/widgets/LogPanel.h"

#include <QApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

SerialProtocolPage::SerialProtocolPage(int defaultBaud, QWidget* parent)
    : IProtocolPage(parent)
{
    m_connectPanel = new ConnectConfigPanel(ProtocolConnectMode::SerialOnly, this, defaultBaud);
    m_transport = new SerialTransport(); // 刻意不挂父对象：驻留自身线程

    // 连接面板 -> 传输。
    connect(m_connectPanel, &ConnectConfigPanel::portOpened,
            this, &SerialProtocolPage::onPortOpened);
    connect(m_connectPanel, &ConnectConfigPanel::portClosed,
            this, &SerialProtocolPage::onPortClosed);

    // 传输 -> 页面。
    connect(m_transport, &SerialTransport::bytesReceived,
            this, &SerialProtocolPage::onBytesReceived);
    connect(m_transport, &SerialTransport::connected,
            this, &SerialProtocolPage::onTransportConnected);
    connect(m_transport, &SerialTransport::disconnected,
            this, &SerialProtocolPage::onTransportDisconnected);
    // 串口意外断开（拔线等）时回写连接面板状态，避免“假连接”。
    connect(m_transport, &SerialTransport::disconnected,
            m_connectPanel, &ConnectConfigPanel::onTransportDisconnected);
    connect(m_transport, &SerialTransport::errorOccurred,
            this, &SerialProtocolPage::onTransportError);

    // 发送后无应答超时提示（单次定时器）。
    m_replyTimer = new QTimer(this);
    m_replyTimer->setSingleShot(true);
    m_replyTimer->setInterval(m_replyTimeoutMs);
    connect(m_replyTimer, &QTimer::timeout, this, [this]() {
        if (m_log)
            m_log->append(QStringLiteral("无应答：请检查设备连接或波特率"),
                          QStringLiteral("WARN"));
    });
}

SerialProtocolPage::~SerialProtocolPage()
{
    delete m_transport;
    m_transport = nullptr;
}

void SerialProtocolPage::activate()
{
    if (m_transport)
        m_transport->start();
}

void SerialProtocolPage::deactivate()
{
    if (m_connectPanel && m_connectPanel->isSerialOpen())
        m_connectPanel->setSerialOpenState(false);
    if (m_transport)
        m_transport->stop();
}

void SerialProtocolPage::setReplyTimeoutMs(int ms)
{
    if (ms <= 0)
        return;
    m_replyTimeoutMs = ms;
    if (m_replyTimer)
        m_replyTimer->setInterval(ms);
}

void SerialProtocolPage::setupTabs(QWidget* commandTabs)
{
    auto* tabs = new QTabWidget(this);
    tabs->addTab(m_connectPanel, QStringLiteral("串口"));
    tabs->addTab(commandTabs, QStringLiteral("协议帧生成"));

    // 监视页：接收 + 发送记录。
    auto* monitor = new QWidget(tabs);
    auto* mlay = new QVBoxLayout(monitor);

    m_rxMonitor = new QPlainTextEdit(monitor);
    m_rxMonitor->setReadOnly(true);
    m_rxMonitor->setPlaceholderText(QStringLiteral("接收数据 (RX)"));
    m_rxMonitor->setMaximumBlockCount(5000);

    m_txMonitor = new QPlainTextEdit(monitor);
    m_txMonitor->setReadOnly(true);
    m_txMonitor->setPlaceholderText(QStringLiteral("发送记录 (TX)"));
    m_txMonitor->setMaximumBlockCount(5000);

    auto* clearBtn = new QPushButton(QStringLiteral("清空"), monitor);
    connect(clearBtn, &QPushButton::clicked, this, [this]() {
        if (m_rxMonitor)
            m_rxMonitor->clear();
        if (m_txMonitor)
            m_txMonitor->clear();
    });

    auto* btnRow = new QHBoxLayout();
    btnRow->addWidget(clearBtn);
    btnRow->addStretch(1);

    mlay->addWidget(new QLabel(QStringLiteral("接收监视"), monitor));
    mlay->addWidget(m_rxMonitor, 2);
    mlay->addWidget(new QLabel(QStringLiteral("发送记录"), monitor));
    mlay->addWidget(m_txMonitor, 1);
    mlay->addLayout(btnRow);
    tabs->addTab(monitor, QStringLiteral("监视"));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(tabs);
}

std::function<void()> SerialProtocolPage::addCommandTab(QTabWidget* tabs, const QString& title,
                                                        QWidget* form,
                                                        std::function<QByteArray()> builder,
                                                        bool expectReply)
{
    auto* tab = new QWidget(tabs);
    auto* layout = new QVBoxLayout(tab);
    layout->addWidget(form);

    auto* preview = new QLineEdit(tab);
    preview->setReadOnly(true);
    preview->setPlaceholderText(QStringLiteral("拼接后的协议帧（十六进制）"));

    auto* count = new QLabel(QStringLiteral("0 bytes"), tab);

    auto* previewRow = new QHBoxLayout();
    previewRow->addWidget(preview, 1);
    previewRow->addWidget(count);
    layout->addLayout(previewRow);

    auto* sendBtn = new QPushButton(QStringLiteral("发送"), tab);
    auto* copyBtn = new QPushButton(QStringLiteral("复制帧"), tab);

    auto* btnRow = new QHBoxLayout();
    btnRow->addWidget(sendBtn);
    btnRow->addWidget(copyBtn);
    btnRow->addStretch(1);
    layout->addLayout(btnRow);
    layout->addStretch(1);

    tabs->addTab(tab, title);

    connect(sendBtn, &QPushButton::clicked, this,
            [this, builder, expectReply]() { sendFrame(builder(), expectReply); });
    connect(copyBtn, &QPushButton::clicked, this,
            [this, builder]() { copyFrame(builder()); });

    auto refresh = [preview, count, builder]() {
        const QByteArray frame = builder();
        preview->setText(frameToHex(frame));
        count->setText(QStringLiteral("%1 bytes").arg(frame.size()));
    };
    refresh();

    return refresh;
}

bool SerialProtocolPage::sendFrame(const QByteArray& frame, bool expectReply)
{
    if (!m_connectPanel || !m_connectPanel->isSerialOpen()) {
        if (m_log)
            m_log->append(QStringLiteral("请先打开串口"), QStringLiteral("WARN"));
        return false;
    }

    if (m_transport)
        m_transport->send(frame);
    if (m_txMonitor)
        m_txMonitor->appendPlainText(QStringLiteral("> %1").arg(frameToHex(frame)));
    if (m_log)
        m_log->append(QStringLiteral(">>> TX: %1").arg(frameToHex(frame)), QStringLiteral("CMD"));
    // 单向命令（协议规定设备不应答）：不进入应答等待，日志给中性确认。
    if (!expectReply) {
        if (m_log)
            m_log->append(QStringLiteral("命令已发送（协议无应答）"), QStringLiteral("INFO"));
        return true;
    }
    // 发送成功后重启“无应答”超时定时器。
    if (m_replyTimer)
        m_replyTimer->start();
    return true;
}

void SerialProtocolPage::copyFrame(const QByteArray& frame)
{
    QApplication::clipboard()->setText(frameToHex(frame));
    if (m_log)
        m_log->append(QStringLiteral("帧已复制到剪贴板"), QStringLiteral("INFO"));
}

void SerialProtocolPage::appendRx(const QByteArray& data)
{
    if (m_rxMonitor)
        m_rxMonitor->appendPlainText(QStringLiteral("< %1").arg(frameToHex(data)));
    if (m_log)
        m_log->append(QStringLiteral("<<< RX: %1").arg(frameToHex(data)), QStringLiteral("INFO"));
}

void SerialProtocolPage::onRxData(const QByteArray& data)
{
    Q_UNUSED(data);
}

QString SerialProtocolPage::frameToHex(const QByteArray& frame)
{
    QStringList parts;
    parts.reserve(frame.size());
    for (char c : frame)
        parts << QStringLiteral("%1")
                     .arg(static_cast<quint8>(c), 2, 16, QLatin1Char('0'))
                     .toUpper();
    return parts.join(QLatin1Char(' '));
}

void SerialProtocolPage::onPortOpened(const QString& port, int baud)
{
    if (m_transport)
        m_transport->open(port, baud);
}

void SerialProtocolPage::onPortClosed()
{
    if (m_transport)
        m_transport->close();
}

void SerialProtocolPage::onTransportConnected(const QString& port, int baud)
{
    if (m_log)
        m_log->append(QStringLiteral("%1 串口 %2 @ %3 已打开")
                          .arg(fullName(), port)
                          .arg(baud),
                      QStringLiteral("SUCCESS"));
}

void SerialProtocolPage::onTransportDisconnected()
{
    if (m_log)
        m_log->append(QStringLiteral("%1 串口已关闭").arg(fullName()), QStringLiteral("INFO"));
}

void SerialProtocolPage::onTransportError(const QString& msg)
{
    if (m_log)
        m_log->append(msg, QStringLiteral("ERROR"));
}

void SerialProtocolPage::onBytesReceived(const QByteArray& data)
{
    // 收到任意数据即视为有应答：停止超时定时器。
    if (m_replyTimer)
        m_replyTimer->stop();
    appendRx(data);
    onRxData(data);
}