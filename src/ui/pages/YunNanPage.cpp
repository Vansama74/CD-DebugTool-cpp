#include "YunNanPage.h"

#include "protocol/yunnan/YunNanProtocol.h"
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

YunNanPage::YunNanPage(QWidget* parent)
    : IProtocolPage(parent)
{
    // 云南协议默认波特率 9600（9600~115200 可调，8N1 由 SerialTransport 固定）。
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
            this, &YunNanPage::onPortOpened);
    connect(m_connectPanel, &ConnectConfigPanel::portClosed,
            this, &YunNanPage::onPortClosed);

    // Transport -> page.
    connect(m_transport, &SerialTransport::bytesReceived,
            this, &YunNanPage::onBytesReceived);
    connect(m_transport, &SerialTransport::connected,
            this, &YunNanPage::onTransportConnected);
    connect(m_transport, &SerialTransport::disconnected,
            this, &YunNanPage::onTransportDisconnected);
    connect(m_transport, &SerialTransport::errorOccurred,
            this, &YunNanPage::onTransportError);
}

YunNanPage::~YunNanPage()
{
    delete m_transport;
    m_transport = nullptr;
}

QString YunNanPage::key() const { return QStringLiteral("yunnan"); }
QString YunNanPage::fullName() const { return QStringLiteral("云南费显协议"); }

void YunNanPage::activate()
{
    if (m_transport)
        m_transport->start();
}

void YunNanPage::deactivate()
{
    if (m_connectPanel && m_connectPanel->isSerialOpen())
        m_connectPanel->setSerialOpenState(false);
    if (m_transport)
        m_transport->stop();
}

void YunNanPage::onPortOpened(const QString& port, int baud)
{
    if (m_transport)
        m_transport->open(port, baud);
}

void YunNanPage::onPortClosed()
{
    if (m_transport)
        m_transport->close();
}

void YunNanPage::onTransportConnected(const QString& port, int baud)
{
    if (m_log)
        m_log->append(QStringLiteral("云南串口 %1 @ %2 已打开").arg(port).arg(baud),
                      QStringLiteral("SUCCESS"));
}

void YunNanPage::onTransportDisconnected()
{
    if (m_log)
        m_log->append(QStringLiteral("云南串口已关闭"), QStringLiteral("INFO"));
}

void YunNanPage::onTransportError(const QString& msg)
{
    if (m_log)
        m_log->append(msg, QStringLiteral("ERROR"));
}

void YunNanPage::onBytesReceived(const QByteArray& data)
{
    const bool hadPending = m_parser.hasPending();
    m_parser.feed(data);
    yunnan::Frame frame;
    bool gotFrame = false;
    while (m_parser.next(&frame)) {
        gotFrame = true;
        onFrameReceived(frame);
    }

    // 0x02 版本号应答为无封套裸 ASCII 文本：无悬挂半帧且本块不含 '{' 时按文本展示。
    if (!gotFrame && !hadPending && !data.contains(static_cast<char>(0x7B)))
        showBareTextReply(data);
}

void YunNanPage::showBareTextReply(const QByteArray& data)
{
    const QString text = yunnan::fromGbk(data);
    if (m_rxMonitor)
        m_rxMonitor->appendPlainText(QStringLiteral("< 文本: %1").arg(text));
    if (m_log)
        m_log->append(QStringLiteral("<<< RX 裸文本应答(版本号): %1").arg(text),
                      QStringLiteral("SUCCESS"));
}

void YunNanPage::onFrameReceived(const yunnan::Frame& frame)
{
    const QByteArray full = yunnan::buildFrame(frame.cmd, frame.data);
    appendMonitor(m_rxMonitor, QStringLiteral("<"), full);

    if (m_log) {
        if (!frame.ok)
            m_log->append(QStringLiteral("<<< RX: %1 (未知命令)").arg(frameToHex(full)),
                          QStringLiteral("WARN"));
        else
            m_log->append(QStringLiteral("<<< RX: %1").arg(frameToHex(full)),
                          QStringLiteral("INFO"));
    }

    if (frame.ok && frame.cmd == yunnan::Cmd::HostQuery) {
        const yunnan::QueryReply reply = yunnan::parseQueryReply(frame.data);
        if (reply.ok && m_log) {
            m_log->append(reply.normal ? QStringLiteral("查询应答: 设备正常")
                                       : QStringLiteral("查询应答: 设备异常"),
                          QStringLiteral("SUCCESS"));
        }
    }

    if (frame.ok && frame.cmd == yunnan::Cmd::GetVersion) {
        const QString ver = yunnan::parseVersionReply(full);
        if (!ver.isEmpty() && m_log)
            m_log->append(QStringLiteral("版本号应答: %1").arg(ver), QStringLiteral("SUCCESS"));
    }
}

QString YunNanPage::frameToHex(const QByteArray& frame)
{
    QStringList parts;
    parts.reserve(frame.size());
    for (char c : frame)
        parts << QStringLiteral("%1")
                     .arg(static_cast<quint8>(c), 2, 16, QLatin1Char('0'))
                     .toUpper();
    return parts.join(QLatin1Char(' '));
}

QString YunNanPage::frameToAscii(const QByteArray& frame)
{
    QString ascii;
    ascii.reserve(frame.size());
    for (char c : frame) {
        const quint8 u = static_cast<quint8>(c);
        if (u >= 0x20 && u <= 0x7E)
            ascii.append(QLatin1Char(c));
        else
            ascii.append(QLatin1Char('.'));
    }
    return ascii;
}

void YunNanPage::appendMonitor(QPlainTextEdit* mon, const QString& arrow,
                               const QByteArray& bytes)
{
    if (!mon)
        return;
    mon->appendPlainText(QStringLiteral("%1 %2  | %3")
                             .arg(arrow, frameToHex(bytes), frameToAscii(bytes)));
}

void YunNanPage::onSendCommand(const std::function<QByteArray()>& builder)
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
    appendMonitor(m_txMonitor, QStringLiteral(">"), frame);
    if (m_log)
        m_log->append(QStringLiteral(">>> TX: %1").arg(hex), QStringLiteral("CMD"));
}

void YunNanPage::onCopyCommand(const std::function<QByteArray()>& builder)
{
    const QByteArray frame = builder();
    QApplication::clipboard()->setText(frameToHex(frame));
    if (m_log)
        m_log->append(QStringLiteral("帧已复制到剪贴板"), QStringLiteral("INFO"));
}

QWidget* YunNanPage::buildMonitorTab()
{
    auto* tab = new QWidget(this);
    auto* layout = new QVBoxLayout(tab);

    m_rxMonitor = new QPlainTextEdit(tab);
    m_rxMonitor->setReadOnly(true);
    m_rxMonitor->setPlaceholderText(QStringLiteral("接收数据 (RX) — 十六进制 | ASCII"));

    m_txMonitor = new QPlainTextEdit(tab);
    m_txMonitor->setReadOnly(true);
    m_txMonitor->setPlaceholderText(QStringLiteral("发送记录 (TX) — 十六进制 | ASCII"));

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

std::function<void()> YunNanPage::addCommandTab(QTabWidget* tabs, const QString& title,
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

QWidget* YunNanPage::buildCommandTabs()
{
    auto* tabs = new QTabWidget(this);

    auto colorCombo = [](QWidget* parent) {
        auto* c = new QComboBox(parent);
        c->addItems({QStringLiteral("红色"), QStringLiteral("绿色"), QStringLiteral("黄色")});
        return c;
    };

    // 查询 (HostQuery '1')
    {
        auto* form = new QWidget(tabs);
        auto* lay = new QVBoxLayout(form);
        lay->addWidget(new QLabel(QStringLiteral("查询设备状态，无参数。设备应答 7B 31 01 00 7D（恒回正常）。"), form));
        lay->addStretch(1);
        addCommandTab(tabs, QStringLiteral("查询"), form,
                      []() { return yunnan::buildFrame(yunnan::Cmd::HostQuery, QByteArray()); });
    }

    // 自检 (SelfCheck '2')
    {
        auto* form = new QWidget(tabs);
        auto* lay = new QVBoxLayout(form);
        lay->addWidget(new QLabel(QStringLiteral("触发设备自检（老化循环显示 + 每 5s 语音「系统正在自检」，可被下一帧命令打断），无参数，无应答。"), form));
        lay->addStretch(1);
        addCommandTab(tabs, QStringLiteral("自检"), form,
                      []() { return yunnan::buildFrame(yunnan::Cmd::SelfCheck, QByteArray()); });
    }

    // 单行显示 (OneLine '3')
    {
        auto* box = new QGroupBox(QStringLiteral("单行显示参数"), tabs);
        auto* form = new QFormLayout(box);
        auto* color = colorCombo(box);
        auto* row = new QSpinBox(box);
        row->setRange(1, 5);
        row->setValue(1);
        auto* text = new QLineEdit(box);
        text->setPlaceholderText(QStringLiteral("单行文本（GBK/GB2312，≤12 个 ASCII 或 6 个汉字，超宽设备截断）"));
        form->addRow(QStringLiteral("颜色"), color);
        form->addRow(QStringLiteral("行号"), row);
        form->addRow(QStringLiteral("文本"), text);

        auto refresh = addCommandTab(tabs, QStringLiteral("单行显示"), box,
            [color, row, text]() {
                return yunnan::buildFrame(yunnan::Cmd::OneLine,
                    yunnan::oneLinePayload(color->currentIndex(), row->value(), text->text()));
            });
        connect(color, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [refresh](int) { refresh(); });
        connect(row, QOverload<int>::of(&QSpinBox::valueChanged), this,
                [refresh](int) { refresh(); });
        connect(text, &QLineEdit::textChanged, this,
                [refresh](const QString&) { refresh(); });
    }

    // 全屏可编辑 (FullScreenEdit '4')
    {
        auto* box = new QGroupBox(QStringLiteral("全屏可编辑参数"), tabs);
        auto* form = new QFormLayout(box);
        auto* color = colorCombo(box);
        auto* x = new QSpinBox(box);
        x->setRange(0, 255);
        x->setValue(0);
        auto* y = new QSpinBox(box);
        y->setRange(0, 255);
        y->setValue(0);
        auto* text = new QLineEdit(box);
        text->setPlaceholderText(QStringLiteral("全屏文本（GBK/GB2312 编码）"));
        form->addRow(QStringLiteral("颜色"), color);
        form->addRow(QStringLiteral("X"), x);
        form->addRow(QStringLiteral("Y"), y);
        form->addRow(QStringLiteral("文本"), text);

        auto refresh = addCommandTab(tabs, QStringLiteral("全屏可编辑"), box,
            [color, x, y, text]() {
                return yunnan::buildFrame(yunnan::Cmd::FullScreenEdit,
                    yunnan::fullScreenEditPayload(color->currentIndex(),
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

    // 全屏清除 (ClearAll '5')
    {
        auto* form = new QWidget(tabs);
        auto* lay = new QVBoxLayout(form);
        lay->addWidget(new QLabel(QStringLiteral("全屏清除显示，无参数，无应答。"), form));
        lay->addStretch(1);
        addCommandTab(tabs, QStringLiteral("全屏清除"), form,
                      []() { return yunnan::buildFrame(yunnan::Cmd::ClearAll, QByteArray()); });
    }

    // 单行清除 (ClearLine '6')
    {
        auto* box = new QGroupBox(QStringLiteral("单行清除参数"), tabs);
        auto* form = new QFormLayout(box);
        auto* row = new QSpinBox(box);
        row->setRange(1, 5);
        row->setValue(1);
        form->addRow(QStringLiteral("行号"), row);

        auto refresh = addCommandTab(tabs, QStringLiteral("单行清除"), box,
            [row]() {
                return yunnan::buildFrame(yunnan::Cmd::ClearLine,
                    yunnan::clearLinePayload(row->value()));
            });
        connect(row, QOverload<int>::of(&QSpinBox::valueChanged), this,
                [refresh](int) { refresh(); });
    }

    // 礼貌语音 (CivilVoice '7')
    {
        auto* box = new QGroupBox(QStringLiteral("礼貌语音"), tabs);
        auto* form = new QFormLayout(box);
        auto* index = new QComboBox(box);
        index->addItems({QStringLiteral("'0' 祝您旅途愉快"), QStringLiteral("'1' 请出示通行卡"),
                         QStringLiteral("'2' 谢谢合作、祝您一路平安"),
                         QStringLiteral("'3' 交易不成功，请走人工车道")});
        form->addRow(QStringLiteral("语音"), index);

        auto refresh = addCommandTab(tabs, QStringLiteral("礼貌语音"), box,
            [index]() {
                return yunnan::buildFrame(yunnan::Cmd::CivilVoice,
                    yunnan::civilVoicePayload(index->currentIndex()));
            });
        connect(index, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [refresh](int) { refresh(); });
    }

    // 亮度 (Brightness '8')
    {
        auto* box = new QGroupBox(QStringLiteral("亮度调节"), tabs);
        auto* form = new QFormLayout(box);
        auto* level = new QComboBox(box);
        level->addItems({QStringLiteral("自动 (0x00)"), QStringLiteral("1"),
                         QStringLiteral("2"), QStringLiteral("3"), QStringLiteral("4"),
                         QStringLiteral("5"), QStringLiteral("6"), QStringLiteral("7"),
                         QStringLiteral("8 (最亮)")});
        form->addRow(QStringLiteral("亮度档位"), level);

        auto refresh = addCommandTab(tabs, QStringLiteral("亮度"), box,
            [level]() {
                return yunnan::buildFrame(yunnan::Cmd::Brightness,
                    yunnan::brightnessPayload(level->currentIndex()));
            });
        connect(level, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [refresh](int) { refresh(); });
    }

    // 音量 (Volume '9')
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
                return yunnan::buildFrame(yunnan::Cmd::Volume,
                    yunnan::volumePayload(level->currentIndex() + 1));
            });
        connect(level, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [refresh](int) { refresh(); });
    }

    // 外设 (Peripheral 'A')
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
                return yunnan::buildFrame(yunnan::Cmd::Peripheral,
                    yunnan::peripheralPayload(green->isChecked(), red->isChecked(),
                                              yellow->isChecked()));
            });
        connect(green, &QCheckBox::toggled, this, [refresh](bool) { refresh(); });
        connect(red, &QCheckBox::toggled, this, [refresh](bool) { refresh(); });
        connect(yellow, &QCheckBox::toggled, this, [refresh](bool) { refresh(); });
    }

    // 费额语音 (FeeVoice 'B')
    {
        auto* box = new QGroupBox(QStringLiteral("费额语音参数"), tabs);
        auto* form = new QFormLayout(box);
        auto* amount = new QDoubleSpinBox(box);
        amount->setRange(0.0, 99999.99);
        amount->setDecimals(2);
        amount->setValue(123.40);
        form->addRow(QStringLiteral("金额(元)"), amount);

        auto refresh = addCommandTab(tabs, QStringLiteral("费额语音"), box,
            [amount]() {
                QString s = QString::number(amount->value(), 'f', 2);
                while (s.endsWith(QLatin1Char('0')))
                    s.chop(1);
                if (s.endsWith(QLatin1Char('.')))
                    s.chop(1);
                return yunnan::buildFrame(yunnan::Cmd::FeeVoice,
                    yunnan::feeVoicePayload(s));
            });
        connect(amount, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [refresh](double) { refresh(); });
    }

    // 全屏点亮 (FullScreenLight 0x01，设备扩展七色)
    {
        auto* box = new QGroupBox(QStringLiteral("全屏点亮颜色（设备扩展七色）"), tabs);
        auto* form = new QFormLayout(box);
        auto* color = new QComboBox(box);
        color->addItems({QStringLiteral("01 红"), QStringLiteral("02 绿"),
                         QStringLiteral("03 黄"), QStringLiteral("04 蓝"),
                         QStringLiteral("05 紫"), QStringLiteral("06 青"),
                         QStringLiteral("07 白")});
        form->addRow(QStringLiteral("颜色"), color);

        auto refresh = addCommandTab(tabs, QStringLiteral("全屏点亮"), box,
            [color]() {
                return yunnan::buildFrame(yunnan::Cmd::FullScreenLight,
                    yunnan::fullScreenLightPayload(color->currentIndex() + 1));
            });
        connect(color, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [refresh](int) { refresh(); });
    }

    // 版本号 (GetVersion 0x02)
    {
        auto* form = new QWidget(tabs);
        auto* lay = new QVBoxLayout(form);
        lay->addWidget(new QLabel(QStringLiteral("发送 7B 02 01 00 7D 获取版本号。"), form));
        lay->addWidget(new QLabel(QStringLiteral("设备应答裸 ASCII 版本号文本（设备侧回固件 PROGRAM_CODE；协议文档约定 YN_FX_P5_1.0）。"), form));
        lay->addStretch(1);
        addCommandTab(tabs, QStringLiteral("版本号"), form,
                      []() { return yunnan::buildFrame(yunnan::Cmd::GetVersion,
                                                       yunnan::versionPayload()); });
    }

    return tabs;
}