#include "QingHaiPage.h"

#include "protocol/qinghai/QingHaiProtocol.h"
#include "transport/SerialTransport.h"
#include "ui/widgets/ConnectConfigPanel.h"
#include "ui/widgets/LogPanel.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStringList>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QtMath>

QingHaiPage::QingHaiPage(QWidget* parent)
    : IProtocolPage(parent)
{
    m_connectPanel = new ConnectConfigPanel(ProtocolConnectMode::SerialOnly, this, 9600);
    m_transport = new SerialTransport(); // deliberately unparented

    auto* tabs = new QTabWidget(this);
    tabs->addTab(m_connectPanel, QStringLiteral("串口"));
    tabs->addTab(buildCommandTabs(), QStringLiteral("协议帧生成"));
    tabs->addTab(buildMonitorTab(), QStringLiteral("监视"));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(tabs);

    // Connection config -> transport.
    connect(m_connectPanel, &ConnectConfigPanel::portOpened,
            this, &QingHaiPage::onPortOpened);
    connect(m_connectPanel, &ConnectConfigPanel::portClosed,
            this, &QingHaiPage::onPortClosed);

    // Transport -> page.
    connect(m_transport, &SerialTransport::bytesReceived,
            this, &QingHaiPage::onBytesReceived);
    connect(m_transport, &SerialTransport::connected,
            this, &QingHaiPage::onTransportConnected);
    connect(m_transport, &SerialTransport::disconnected,
            this, &QingHaiPage::onTransportDisconnected);
    connect(m_transport, &SerialTransport::errorOccurred,
            this, &QingHaiPage::onTransportError);
}

QingHaiPage::~QingHaiPage()
{
    delete m_transport;
    m_transport = nullptr;
}

QString QingHaiPage::key() const { return QStringLiteral("qinghai"); }
QString QingHaiPage::fullName() const { return QStringLiteral("青海高速费显协议"); }

void QingHaiPage::activate()
{
    if (m_transport)
        m_transport->start();
}

void QingHaiPage::deactivate()
{
    if (m_connectPanel && m_connectPanel->isSerialOpen())
        m_connectPanel->setSerialOpenState(false);
    if (m_transport)
        m_transport->stop();
}

void QingHaiPage::onPortOpened(const QString& port, int baud)
{
    if (m_transport)
        m_transport->open(port, baud);
}

void QingHaiPage::onPortClosed()
{
    if (m_transport)
        m_transport->close();
}

void QingHaiPage::onTransportConnected(const QString& port, int baud)
{
    if (m_log)
        m_log->append(QStringLiteral("青海串口 %1 @ %2 已打开").arg(port).arg(baud),
                      QStringLiteral("SUCCESS"));
}

void QingHaiPage::onTransportDisconnected()
{
    if (m_log)
        m_log->append(QStringLiteral("青海串口已关闭"), QStringLiteral("INFO"));
}

void QingHaiPage::onTransportError(const QString& msg)
{
    if (m_log)
        m_log->append(msg, QStringLiteral("ERROR"));
}

void QingHaiPage::onBytesReceived(const QByteArray& data)
{
    m_parser.feed(data);
    qinghai::Frame frame;
    while (m_parser.next(&frame))
        onFrameReceived(frame);
}

void QingHaiPage::onFrameReceived(const qinghai::Frame& frame)
{
    const QByteArray full = qinghai::buildFrame(frame.cmd, frame.data);
    const QString hex = frameToHex(full);

    if (m_rxMonitor)
        m_rxMonitor->appendPlainText(QStringLiteral("< %1").arg(hex));
    if (m_log) {
        if (!frame.ok)
            m_log->append(QStringLiteral("<<< RX: %1 (未知命令)").arg(hex), QStringLiteral("WARN"));
        else
            m_log->append(QStringLiteral("<<< RX: %1").arg(hex), QStringLiteral("INFO"));
    }

    if (frame.ok && frame.cmd == qinghai::Cmd::HostQuery) {
        const qinghai::QueryReply reply = qinghai::parseQueryReply(frame.data);
        if (reply.ok && m_log) {
            m_log->append(reply.normal ? QStringLiteral("查询应答: 设备正常")
                                       : QStringLiteral("查询应答: 设备异常"),
                          QStringLiteral("SUCCESS"));
        }
    }
}

QString QingHaiPage::frameToHex(const QByteArray& frame)
{
    QStringList parts;
    parts.reserve(frame.size());
    for (char c : frame)
        parts << QStringLiteral("%1")
                     .arg(static_cast<quint8>(c), 2, 16, QLatin1Char('0'))
                     .toUpper();
    return parts.join(QLatin1Char(' '));
}

void QingHaiPage::onSendCommand(const std::function<QByteArray()>& builder)
{
    const QByteArray frame = builder();
    const QString hex = frameToHex(frame);

    if (!m_connectPanel || !m_connectPanel->isSerialOpen()) {
        if (m_log)
            m_log->append(QStringLiteral("请先打开串口"), QStringLiteral("WARN"));
        return;
    }

    if (m_transport)
        m_transport->send(frame);
    if (m_txMonitor)
        m_txMonitor->appendPlainText(QStringLiteral("> %1").arg(hex));
    if (m_log)
        m_log->append(QStringLiteral(">>> TX: %1").arg(hex), QStringLiteral("CMD"));
}

void QingHaiPage::onCopyCommand(const std::function<QByteArray()>& builder)
{
    const QByteArray frame = builder();
    QApplication::clipboard()->setText(frameToHex(frame));
    if (m_log)
        m_log->append(QStringLiteral("帧已复制到剪贴板"), QStringLiteral("INFO"));
}

QWidget* QingHaiPage::buildMonitorTab()
{
    auto* tab = new QWidget(this);
    auto* layout = new QVBoxLayout(tab);

    m_rxMonitor = new QPlainTextEdit(tab);
    m_rxMonitor->setReadOnly(true);
    m_rxMonitor->setPlaceholderText(QStringLiteral("接收数据 (RX)"));

    m_txMonitor = new QPlainTextEdit(tab);
    m_txMonitor->setReadOnly(true);
    m_txMonitor->setPlaceholderText(QStringLiteral("发送记录 (TX)"));

    auto* clearBtn = new QPushButton(QStringLiteral("清空"), tab);
    connect(clearBtn, &QPushButton::clicked, this, [this]() {
        if (m_rxMonitor)
            m_rxMonitor->clear();
        if (m_txMonitor)
            m_txMonitor->clear();
    });

    auto* btnRow = new QHBoxLayout();
    btnRow->addWidget(clearBtn);
    btnRow->addStretch(1);

    layout->addWidget(new QLabel(QStringLiteral("接收监视"), tab));
    layout->addWidget(m_rxMonitor, 2);
    layout->addWidget(new QLabel(QStringLiteral("发送记录"), tab));
    layout->addWidget(m_txMonitor, 1);
    layout->addLayout(btnRow);

    return tab;
}

std::function<void()> QingHaiPage::addCommandTab(QTabWidget* tabs, const QString& title,
                                                 QWidget* form,
                                                 std::function<QByteArray()> builder)
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
            [this, builder]() { onSendCommand(builder); });
    connect(copyBtn, &QPushButton::clicked, this,
            [this, builder]() { onCopyCommand(builder); });

    auto refresh = [preview, count, builder]() {
        const QByteArray frame = builder();
        preview->setText(frameToHex(frame));
        count->setText(QStringLiteral("%1 bytes").arg(frame.size()));
    };
    refresh();

    return refresh;
}

QWidget* QingHaiPage::buildCommandTabs()
{
    auto* tabs = new QTabWidget(this);

    auto colorCombo = [](QWidget* parent) {
        auto* c = new QComboBox(parent);
        c->addItems({QStringLiteral("红色"), QStringLiteral("绿色"), QStringLiteral("黄色")});
        return c;
    };

    // 查询 (HostQuery)
    {
        auto* form = new QWidget(tabs);
        auto* lay = new QVBoxLayout(form);
        lay->addWidget(new QLabel(QStringLiteral("查询设备状态，无参数。设备返回 7B 31 01 00 7D。"), form));
        lay->addStretch(1);
        addCommandTab(tabs, QStringLiteral("查询"), form,
                      []() { return qinghai::buildFrame(qinghai::Cmd::HostQuery, QByteArray()); });
    }

    // 自检 (SelfCheck)
    {
        auto* form = new QWidget(tabs);
        auto* lay = new QVBoxLayout(form);
        lay->addWidget(new QLabel(QStringLiteral("触发设备自检（清屏并播报提示音），无参数，无应答。"), form));
        lay->addStretch(1);
        addCommandTab(tabs, QStringLiteral("自检"), form,
                      []() { return qinghai::buildFrame(qinghai::Cmd::SelfCheck, QByteArray()); });
    }

    // 单行显示 (OneLine)
    {
        auto* box = new QGroupBox(QStringLiteral("单行显示参数"), tabs);
        auto* form = new QFormLayout(box);
        auto* color = colorCombo(box);
        auto* row = new QSpinBox(box);
        row->setRange(1, 5);
        row->setValue(1);
        auto* text = new QLineEdit(box);
        text->setPlaceholderText(QStringLiteral("请输入单行文本（GBK 编码）"));
        form->addRow(QStringLiteral("颜色"), color);
        form->addRow(QStringLiteral("行号"), row);
        form->addRow(QStringLiteral("文本"), text);

        auto refresh = addCommandTab(tabs, QStringLiteral("单行显示"), box,
            [color, row, text]() {
                return qinghai::buildFrame(qinghai::Cmd::OneLine,
                    qinghai::oneLinePayload(color->currentIndex(), row->value(), text->text()));
            });
        connect(color, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [refresh](int) { refresh(); });
        connect(row, QOverload<int>::of(&QSpinBox::valueChanged), this,
                [refresh](int) { refresh(); });
        connect(text, &QLineEdit::textChanged, this,
                [refresh](const QString&) { refresh(); });
    }

    // 全屏显示 (FullScreen)
    {
        auto* box = new QGroupBox(QStringLiteral("全屏显示参数"), tabs);
        auto* form = new QFormLayout(box);
        auto* color = colorCombo(box);
        auto* x = new QSpinBox(box);
        x->setRange(0, 255);
        x->setValue(0);
        auto* y = new QSpinBox(box);
        y->setRange(0, 255);
        y->setValue(0);
        auto* text = new QLineEdit(box);
        text->setPlaceholderText(QStringLiteral("请输入全屏文本（GBK 编码）"));
        form->addRow(QStringLiteral("颜色"), color);
        form->addRow(QStringLiteral("X"), x);
        form->addRow(QStringLiteral("Y"), y);
        form->addRow(QStringLiteral("文本"), text);

        auto refresh = addCommandTab(tabs, QStringLiteral("全屏显示"), box,
            [color, x, y, text]() {
                return qinghai::buildFrame(qinghai::Cmd::FullScreen,
                    qinghai::fullScreenPayload(color->currentIndex(),
                        static_cast<quint8>(x->value()), static_cast<quint8>(y->value()),
                        text->text()));
            });
        connect(color, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [refresh](int) { refresh(); });
        connect(x, QOverload<int>::of(&QSpinBox::valueChanged), this,
                [refresh](int) { refresh(); });
        connect(y, QOverload<int>::of(&QSpinBox::valueChanged), this,
                [refresh](int) { refresh(); });
        connect(text, &QLineEdit::textChanged, this,
                [refresh](const QString&) { refresh(); });
    }

    // 清屏 (Clear)
    {
        auto* form = new QWidget(tabs);
        auto* lay = new QVBoxLayout(form);
        lay->addWidget(new QLabel(QStringLiteral("清空显示屏，无参数。"), form));
        lay->addStretch(1);
        addCommandTab(tabs, QStringLiteral("清屏"), form,
                      []() { return qinghai::buildFrame(qinghai::Cmd::Clear, QByteArray()); });
    }

    // 固定显示 (FixedDisplay)
    {
        auto* box = new QGroupBox(QStringLiteral("固定显示参数"), tabs);
        auto* form = new QFormLayout(box);
        auto* type = new QComboBox(box);
        type->addItems({QStringLiteral("客车"), QStringLiteral("货车")});
        auto* text = new QLineEdit(box);
        text->setPlaceholderText(QStringLiteral("字段以 | 分隔，例如：客车|5.00|3.00|..."));
        form->addRow(QStringLiteral("类型"), type);
        form->addRow(QStringLiteral("字段"), text);

        auto refresh = addCommandTab(tabs, QStringLiteral("固定显示"), box,
            [type, text]() {
                return qinghai::buildFrame(qinghai::Cmd::FixedDisplay,
                    qinghai::fixedDisplayPayload(type->currentIndex(), text->text()));
            });
        connect(type, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [refresh](int) { refresh(); });
        connect(text, &QLineEdit::textChanged, this,
                [refresh](const QString&) { refresh(); });
    }

    // 文明语音 (CivilVoice)
    {
        auto* box = new QGroupBox(QStringLiteral("文明语音"), tabs);
        auto* form = new QFormLayout(box);
        auto* index = new QSpinBox(box);
        index->setRange(0, 3);
        index->setValue(0);
        form->addRow(QStringLiteral("语音索引"), index);

        auto refresh = addCommandTab(tabs, QStringLiteral("文明语音"), box,
            [index]() {
                return qinghai::buildFrame(qinghai::Cmd::CivilVoice,
                    qinghai::indexPayload(index->value()));
            });
        connect(index, QOverload<int>::of(&QSpinBox::valueChanged), this,
                [refresh](int) { refresh(); });
    }

    // 亮度 (Brightness)
    {
        auto* box = new QGroupBox(QStringLiteral("亮度调节"), tabs);
        auto* form = new QFormLayout(box);
        auto* level = new QComboBox(box);
        level->addItems({QStringLiteral("0 (自动)"), QStringLiteral("1"), QStringLiteral("2"),
                         QStringLiteral("3"), QStringLiteral("4"), QStringLiteral("5")});
        level->setCurrentIndex(3);
        form->addRow(QStringLiteral("亮度等级"), level);

        auto refresh = addCommandTab(tabs, QStringLiteral("亮度"), box,
            [level]() {
                return qinghai::buildFrame(qinghai::Cmd::Brightness,
                    qinghai::levelPayload(level->currentIndex()));
            });
        connect(level, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [refresh](int) { refresh(); });
    }

    // 音量 (Volume)
    {
        auto* box = new QGroupBox(QStringLiteral("音量调节"), tabs);
        auto* form = new QFormLayout(box);
        auto* level = new QComboBox(box);
        level->addItems({QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("3"),
                         QStringLiteral("4"), QStringLiteral("5")});
        level->setCurrentIndex(2);
        form->addRow(QStringLiteral("音量等级"), level);

        auto refresh = addCommandTab(tabs, QStringLiteral("音量"), box,
            [level]() {
                return qinghai::buildFrame(qinghai::Cmd::Volume,
                    qinghai::levelPayload(level->currentIndex() + 1));
            });
        connect(level, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [refresh](int) { refresh(); });
    }

    // 外设 (Peripheral)
    {
        auto* box = new QGroupBox(QStringLiteral("外设控制"), tabs);
        auto* lay = new QHBoxLayout(box);
        auto* green = new QCheckBox(QStringLiteral("绿灯"), box);
        auto* red = new QCheckBox(QStringLiteral("红灯"), box);
        auto* yellow = new QCheckBox(QStringLiteral("黄闪"), box);
        lay->addWidget(green);
        lay->addWidget(red);
        lay->addWidget(yellow);
        lay->addStretch(1);

        auto refresh = addCommandTab(tabs, QStringLiteral("外设"), box,
            [green, red, yellow]() {
                return qinghai::buildFrame(qinghai::Cmd::Peripheral,
                    qinghai::peripheralPayload(green->isChecked(), red->isChecked(),
                                               yellow->isChecked()));
            });
        connect(green, &QCheckBox::toggled, this, [refresh](bool) { refresh(); });
        connect(red, &QCheckBox::toggled, this, [refresh](bool) { refresh(); });
        connect(yellow, &QCheckBox::toggled, this, [refresh](bool) { refresh(); });
    }

    // 费额语音 (Voice)
    {
        auto* box = new QGroupBox(QStringLiteral("费额语音参数"), tabs);
        auto* form = new QFormLayout(box);
        auto* type = new QSpinBox(box);
        type->setRange(0, 9);
        type->setValue(0);
        auto* amount = new QDoubleSpinBox(box);
        amount->setRange(0.0, 99999.99);
        amount->setDecimals(2);
        amount->setValue(5.00);
        form->addRow(QStringLiteral("类型"), type);
        form->addRow(QStringLiteral("金额(元)"), amount);

        auto refresh = addCommandTab(tabs, QStringLiteral("费额语音"), box,
            [type, amount]() {
                const qint64 fen = qRound64(amount->value() * 100.0);
                return qinghai::buildFrame(qinghai::Cmd::Voice,
                    qinghai::feeVoicePayload(type->value(), QString::number(fen)));
            });
        connect(type, QOverload<int>::of(&QSpinBox::valueChanged), this,
                [refresh](int) { refresh(); });
        connect(amount, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [refresh](double) { refresh(); });
    }

    return tabs;
}
