#include "SiChuanMtcPage.h"

#include "ui/widgets/LogPanel.h"

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

SiChuanMtcPage::SiChuanMtcPage(QWidget* parent)
    : SerialProtocolPage(115200, parent) // MTC 默认波特率 115200
{
    auto* cmdTabs = buildCommandTabs();
    setupTabs(cmdTabs);

    // 版本号等无封套文本应答可能在一次串口突发内完整到达（无终止字节），
    // 静默 300ms 后冲刷扫描器缓冲兜底。
    m_flushTimer = new QTimer(this);
    m_flushTimer->setSingleShot(true);
    m_flushTimer->setInterval(300);
    connect(m_flushTimer, &QTimer::timeout, this, [this]() {
        sc_mtc::Reply reply;
        while (m_replyScanner.flush(&reply))
            handleReply(reply);
    });
}

void SiChuanMtcPage::onRxData(const QByteArray& data)
{
    m_replyScanner.feed(data);
    sc_mtc::Reply reply;
    while (m_replyScanner.next(&reply))
        handleReply(reply);
    if (m_flushTimer)
        m_flushTimer->start();
}

void SiChuanMtcPage::handleReply(const sc_mtc::Reply& reply)
{
    if (!reply.valid || !m_log)
        return;
    switch (reply.kind) {
    case sc_mtc::Reply::HostQuery:
        m_log->append(reply.hostNormal ? QStringLiteral("查询应答: 设备状态正常 (0A 64 0A)")
                                       : QStringLiteral("查询应答: 设备状态不正常 (0A 64 00)"),
                      reply.hostNormal ? QStringLiteral("SUCCESS") : QStringLiteral("ERROR"));
        break;
    case sc_mtc::Reply::Text:
        m_log->append(QStringLiteral("设备返回文本: %1").arg(reply.text),
                      QStringLiteral("SUCCESS"));
        break;
    default:
        break;
    }
}

QWidget* SiChuanMtcPage::buildCommandTabs()
{
    auto* tabs = new QTabWidget(this);

    // 初始化
    {
        auto* form = new QWidget(tabs);
        auto* lay = new QVBoxLayout(form);
        lay->addWidget(new QLabel(
            QStringLiteral("显示「祝您一路平安」并语音播报，稍候熄灭。无参数。"), form));
        lay->addStretch(1);
        addCommandTab(tabs, QStringLiteral("初始化"), form,
                      []() { return sc_mtc::initFrame(); });
    }

    // 自检
    {
        auto* form = new QWidget(tabs);
        auto* lay = new QVBoxLayout(form);
        lay->addWidget(new QLabel(
            QStringLiteral("进入自动检测：固定汉字信息及数字 1~9 交替显示，\n"
                           "播报示例语音内容。无参数。"),
            form));
        lay->addStretch(1);
        addCommandTab(tabs, QStringLiteral("自检"), form,
                      []() { return sc_mtc::selfCheckFrame(); });
    }

    // 单行显示
    {
        auto* box = new QGroupBox(QStringLiteral("单行显示参数"), tabs);
        auto* form = new QFormLayout(box);
        auto* row = new QSpinBox(box);
        row->setRange(1, 4);
        row->setValue(1);
        auto* text = new QLineEdit(box);
        text->setPlaceholderText(QStringLiteral("单行文本（GBK，≤8 汉字，不足补空格）"));
        text->setText(QStringLiteral("四川欢迎您"));
        form->addRow(QStringLiteral("行号"), row);
        form->addRow(QStringLiteral("文本"), text);

        auto refresh = addCommandTab(tabs, QStringLiteral("单行显示"), box,
            [row, text]() { return sc_mtc::oneLineFrame(row->value(), text->text()); });
        connect(row, QOverload<int>::of(&QSpinBox::valueChanged), this,
                [refresh](int) { refresh(); });
        connect(text, &QLineEdit::textChanged, this,
                [refresh](const QString&) { refresh(); });
    }

    // 全屏显示
    {
        auto* box = new QGroupBox(QStringLiteral("全屏显示参数"), tabs);
        auto* form = new QFormLayout(box);
        auto* text = new QLineEdit(box);
        text->setPlaceholderText(QStringLiteral("全屏文本（GBK，≤32 汉字，不足补空格）"));
        text->setText(QStringLiteral("四川省高速公路欢迎您"));
        form->addRow(QStringLiteral("文本"), text);

        auto refresh = addCommandTab(tabs, QStringLiteral("全屏显示"), box,
            [text]() { return sc_mtc::fullScreenFrame(text->text()); });
        connect(text, &QLineEdit::textChanged, this,
                [refresh](const QString&) { refresh(); });
    }

    // 清屏
    {
        auto* form = new QWidget(tabs);
        auto* lay = new QVBoxLayout(form);
        lay->addWidget(new QLabel(QStringLiteral("全屏清除显示。无参数。"), form));
        lay->addStretch(1);
        addCommandTab(tabs, QStringLiteral("清屏"), form,
                      []() { return sc_mtc::clearFrame(); });
    }

    // 固定显示（客车）
    {
        auto* box = new QGroupBox(QStringLiteral("客车固定显示"), tabs);
        auto* form = new QFormLayout(box);
        auto* vtype = new QSpinBox(box);
        vtype->setRange(1, 9);
        vtype->setValue(1);
        auto* amount = new QSpinBox(box);
        amount->setRange(0, 99999);
        amount->setValue(5);
        amount->setSuffix(QStringLiteral(" 元"));
        auto* balance = new QSpinBox(box);
        balance->setRange(0, 99999);
        balance->setValue(20);
        balance->setSuffix(QStringLiteral(" 元"));
        form->addRow(QStringLiteral("车型 (1~9)"), vtype);
        form->addRow(QStringLiteral("金额 (万元内)"), amount);
        form->addRow(QStringLiteral("余额 (万元内)"), balance);

        auto refresh = addCommandTab(tabs, QStringLiteral("固定显示(客车)"), box,
            [vtype, amount, balance]() {
                return sc_mtc::fixedBusFrame(vtype->value(), amount->value(), balance->value());
            });
        connect(vtype, QOverload<int>::of(&QSpinBox::valueChanged), this,
                [refresh](int) { refresh(); });
        connect(amount, QOverload<int>::of(&QSpinBox::valueChanged), this,
                [refresh](int) { refresh(); });
        connect(balance, QOverload<int>::of(&QSpinBox::valueChanged), this,
                [refresh](int) { refresh(); });
    }

    // 固定显示（货车）
    {
        auto* box = new QGroupBox(QStringLiteral("货车固定显示"), tabs);
        auto* form = new QFormLayout(box);
        auto* weight = new QLineEdit(box);
        weight->setPlaceholderText(QStringLiteral("如 12.34"));
        weight->setText(QStringLiteral("12.34"));
        auto* amount = new QSpinBox(box);
        amount->setRange(0, 99999);
        amount->setValue(55);
        amount->setSuffix(QStringLiteral(" 元"));
        auto* balance = new QSpinBox(box);
        balance->setRange(0, 99999);
        balance->setValue(100);
        balance->setSuffix(QStringLiteral(" 元"));
        auto* over = new QLineEdit(box);
        over->setPlaceholderText(QStringLiteral("如 1.20"));
        over->setText(QStringLiteral("1.20"));
        form->addRow(QStringLiteral("总重 (吨)"), weight);
        form->addRow(QStringLiteral("金额 (元)"), amount);
        form->addRow(QStringLiteral("余额 (元)"), balance);
        form->addRow(QStringLiteral("超重 (吨)"), over);

        auto refresh = addCommandTab(tabs, QStringLiteral("固定显示(货车)"), box,
            [weight, amount, balance, over]() {
                return sc_mtc::fixedTruckFrame(weight->text(), amount->value(),
                                               balance->value(), over->text());
            });
        connect(weight, &QLineEdit::textChanged, this,
                [refresh](const QString&) { refresh(); });
        connect(amount, QOverload<int>::of(&QSpinBox::valueChanged), this,
                [refresh](int) { refresh(); });
        connect(balance, QOverload<int>::of(&QSpinBox::valueChanged), this,
                [refresh](int) { refresh(); });
        connect(over, &QLineEdit::textChanged, this,
                [refresh](const QString&) { refresh(); });
    }

    // 语音
    {
        auto* box = new QGroupBox(QStringLiteral("语音播报"), tabs);
        auto* form = new QFormLayout(box);
        auto* idx = new QComboBox(box);
        idx->addItem(QStringLiteral("0 您好、X型车、请交费X元、谢谢合作、一路平安"));
        idx->addItem(QStringLiteral("1 您好、总重X吨、超重X吨、请交费X元"));
        idx->addItem(QStringLiteral("2 您好、X车型"));
        idx->addItem(QStringLiteral("3 总重X吨、超重X吨、金额X元、谢谢合作、一路平安"));
        idx->addItem(QStringLiteral("4 谢谢合作、祝您一路平安"));
        idx->addItem(QStringLiteral("5 月票车，请通行"));
        idx->addItem(QStringLiteral("6 免费车，请通行"));
        idx->addItem(QStringLiteral("7 车辆闯关"));
        idx->addItem(QStringLiteral("8 自定义语音 (GBK 文本)"));
        auto* text = new QLineEdit(box);
        text->setPlaceholderText(QStringLiteral("自定义播报内容（GBK 编码，选 8 时生效）"));
        text->setEnabled(false);
        form->addRow(QStringLiteral("语音"), idx);
        form->addRow(QStringLiteral("自定义内容"), text);

        auto refresh = addCommandTab(tabs, QStringLiteral("语音"), box,
            [idx, text]() {
                return (idx->currentIndex() == 8) ? sc_mtc::customVoiceFrame(text->text())
                                                  : sc_mtc::voiceFrame(idx->currentIndex());
            });
        connect(idx, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [refresh, text](int i) {
                    text->setEnabled(i == 8);
                    refresh();
                });
        connect(text, &QLineEdit::textChanged, this,
                [refresh](const QString&) { refresh(); });
    }

    // 亮度
    {
        auto* box = new QGroupBox(QStringLiteral("亮度设定"), tabs);
        auto* form = new QFormLayout(box);
        auto* level = new QSpinBox(box);
        level->setRange(0, 8);
        level->setValue(8);
        level->setSpecialValueText(QStringLiteral("0 (自动调节)"));
        form->addRow(QStringLiteral("亮度级别"), level);

        auto refresh = addCommandTab(tabs, QStringLiteral("亮度"), box,
            [level]() { return sc_mtc::brightnessFrame(level->value()); });
        connect(level, QOverload<int>::of(&QSpinBox::valueChanged), this,
                [refresh](int) { refresh(); });
    }

    // 音量
    {
        auto* box = new QGroupBox(QStringLiteral("音量设定"), tabs);
        auto* form = new QFormLayout(box);
        auto* level = new QSpinBox(box);
        level->setRange(1, 5);
        level->setValue(5);
        form->addRow(QStringLiteral("音量级别"), level);

        auto refresh = addCommandTab(tabs, QStringLiteral("音量"), box,
            [level]() { return sc_mtc::volumeFrame(level->value()); });
        connect(level, QOverload<int>::of(&QSpinBox::valueChanged), this,
                [refresh](int) { refresh(); });
    }

    // 颜色
    {
        auto* box = new QGroupBox(QStringLiteral("颜色设定"), tabs);
        auto* form = new QFormLayout(box);
        auto* color = new QComboBox(box);
        color->addItem(QStringLiteral("1 红色"), 1);
        color->addItem(QStringLiteral("2 黄色"), 2);
        color->addItem(QStringLiteral("3 绿色"), 3);
        color->setCurrentIndex(0);
        form->addRow(QStringLiteral("显示颜色"), color);

        auto refresh = addCommandTab(tabs, QStringLiteral("颜色"), box,
            [color]() { return sc_mtc::colorFrame(color->currentData().toInt()); });
        connect(color, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [refresh](int) { refresh(); });
    }

    // 主机命令
    {
        auto* box = new QGroupBox(QStringLiteral("主机命令"), tabs);
        auto* form = new QFormLayout(box);
        auto* action = new QComboBox(box);
        action->addItem(QStringLiteral("查询设备状态 (0A 46 0A)"), 0);
        action->addItem(QStringLiteral("清屏 (0A 46 0D)"), 1);
        form->addRow(QStringLiteral("命令"), action);

        auto refresh = addCommandTab(tabs, QStringLiteral("主机命令"), box,
            [action]() {
                return action->currentData().toInt() == 0 ? sc_mtc::hostQueryFrame()
                                                          : sc_mtc::hostClearFrame();
            });
        connect(action, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [refresh](int) { refresh(); });
    }

    // 高级设置（7B 40~45 原始帧族）
    {
        auto* box = new QGroupBox(QStringLiteral("高级设置（创迪科技添加协议）"), tabs);
        auto* form = new QFormLayout(box);
        auto* cmd = new QComboBox(box);
        cmd->addItem(QStringLiteral("40 修改波特率"));
        cmd->addItem(QStringLiteral("41 修改点阵大小"));
        cmd->addItem(QStringLiteral("42 修改字体类型"));
        cmd->addItem(QStringLiteral("43 设置协议类型"));
        cmd->addItem(QStringLiteral("44 全屏点亮"));
        cmd->addItem(QStringLiteral("45 获取版本号"));
        auto* param = new QComboBox(box);
        form->addRow(QStringLiteral("命令"), cmd);
        form->addRow(QStringLiteral("参数"), param);

        const auto repopulate = [cmd, param]() {
            param->clear();
            switch (cmd->currentIndex()) {
            case 0: // 波特率
                param->addItem(QStringLiteral("9600"), 0);
                param->addItem(QStringLiteral("115200"), 1);
                break;
            case 1: // 点阵大小
                param->addItem(QStringLiteral("16 点阵字库"), 0);
                param->addItem(QStringLiteral("24 点阵字库"), 1);
                param->addItem(QStringLiteral("32 点阵字库"), 2);
                break;
            case 2: // 字体
                param->addItem(QStringLiteral("宋体"), 0);
                param->addItem(QStringLiteral("仿宋"), 1);
                param->addItem(QStringLiteral("楷体"), 2);
                param->addItem(QStringLiteral("黑体"), 3);
                break;
            case 3: // 协议类型
                param->addItem(QStringLiteral("治超屏协议"), 0);
                param->addItem(QStringLiteral("ETC协议"), 1);
                param->addItem(QStringLiteral("治超屏协议"), 2);
                break;
            case 4: // 全屏点亮
                param->addItem(QStringLiteral("红色全屏"), 0);
                param->addItem(QStringLiteral("绿色全屏"), 1);
                param->addItem(QStringLiteral("黄色全屏"), 2);
                break;
            default: // 版本号：无参数
                param->addItem(QStringLiteral("（无参数）"), 0);
                break;
            }
        };
        repopulate();

        auto refresh = addCommandTab(tabs, QStringLiteral("高级设置"), box,
            [cmd, param]() {
                const int v = param->currentData().toInt();
                switch (cmd->currentIndex()) {
                case 0: return sc_mtc::rawBaudFrame(v);
                case 1: return sc_mtc::rawDotSizeFrame(v);
                case 2: return sc_mtc::rawFontFrame(v);
                case 3: return sc_mtc::rawProtoFrame(v);
                case 4: return sc_mtc::rawFillAllFrame(v);
                default: return sc_mtc::rawVersionFrame();
                }
            });
        connect(cmd, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [repopulate, refresh](int) {
                    repopulate();
                    refresh();
                });
        connect(param, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [refresh](int) { refresh(); });
    }

    return tabs;
}