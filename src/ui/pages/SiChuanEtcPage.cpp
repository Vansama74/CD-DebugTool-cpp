#include "SiChuanEtcPage.h"

#include "ui/widgets/LogPanel.h"

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

SiChuanEtcPage::SiChuanEtcPage(QWidget* parent)
    : SerialProtocolPage(115200, parent) // ETC 默认波特率 115200
{
    auto* cmdTabs = buildCommandTabs();
    setupTabs(cmdTabs);
}

void SiChuanEtcPage::onRxData(const QByteArray& data)
{
    m_ackScanner.feed(data);
    sc_etc::AckReply reply;
    while (m_ackScanner.next(&reply)) {
        if (!m_log)
            continue;
        switch (reply.kind) {
        case sc_etc::AckReply::Ok:
            m_log->append(QStringLiteral("设备应答: 正常执行该命令"), QStringLiteral("SUCCESS"));
            break;
        case sc_etc::AckReply::TooLong:
            m_log->append(QStringLiteral("设备应答: 数据超长 (0x01)"), QStringLiteral("ERROR"));
            break;
        case sc_etc::AckReply::FrameError:
            m_log->append(QStringLiteral("设备应答: 帧头/帧尾或命令编号错误 (0x02)"),
                          QStringLiteral("ERROR"));
            break;
        default:
            m_log->append(QStringLiteral("设备应答: 未知返回码"), QStringLiteral("WARN"));
            break;
        }
    }
}

QWidget* SiChuanEtcPage::buildCommandTabs()
{
    auto* tabs = new QTabWidget(this);

    // 静态显示（全屏 / 单行）
    {
        auto* box = new QGroupBox(QStringLiteral("静态显示参数"), tabs);
        auto* form = new QFormLayout(box);
        auto* row = new QSpinBox(box);
        row->setRange(0, 6);
        row->setValue(0);
        row->setSpecialValueText(QStringLiteral("0 (全屏)"));
        auto* text = new QLineEdit(box);
        text->setPlaceholderText(QStringLiteral("显示文本（GBK；全屏 ≤28 汉字，单行 ≤12 汉字）"));
        text->setText(QStringLiteral("四川省高速公路欢迎您！"));
        form->addRow(QStringLiteral("行号"), row);
        form->addRow(QStringLiteral("文本"), text);

        auto refresh = addCommandTab(tabs, QStringLiteral("静态显示"), box,
            [row, text]() { return sc_etc::displayFrame(row->value(), text->text()); });
        connect(row, QOverload<int>::of(&QSpinBox::valueChanged), this,
                [refresh](int) { refresh(); });
        connect(text, &QLineEdit::textChanged, this,
                [refresh](const QString&) { refresh(); });
    }

    // 滚屏显示（仅全屏有效）
    {
        auto* box = new QGroupBox(QStringLiteral("滚屏显示参数"), tabs);
        auto* form = new QFormLayout(box);
        auto* md = new QComboBox(box);
        md->addItem(QStringLiteral("00 静态"), 0x00);
        md->addItem(QStringLiteral("01 上滚"), 0x01);
        md->addItem(QStringLiteral("03 左滚"), 0x03);
        md->setCurrentIndex(2);
        auto* rt = new QSpinBox(box);
        rt->setRange(0, 255);
        rt->setValue(2);
        rt->setSuffix(QStringLiteral(" 秒"));
        auto* st = new QSpinBox(box);
        st->setRange(0, 255);
        st->setValue(3);
        st->setSpecialValueText(QStringLiteral("0"));
        auto* text = new QLineEdit(box);
        text->setPlaceholderText(QStringLiteral("滚屏文本（GBK，≤25 汉字）"));
        text->setText(QStringLiteral("四川省高速公路欢迎您！"));
        form->addRow(QStringLiteral("滚动模式"), md);
        form->addRow(QStringLiteral("移动时间"), rt);
        form->addRow(QStringLiteral("停留时间(255=常停)"), st);
        form->addRow(QStringLiteral("文本"), text);

        auto refresh = addCommandTab(tabs, QStringLiteral("滚屏显示"), box,
            [md, rt, st, text]() {
                return sc_etc::scrollFrame(static_cast<quint8>(md->currentData().toInt()),
                                           static_cast<quint8>(rt->value()),
                                           static_cast<quint8>(st->value()),
                                           text->text());
            });
        connect(md, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [refresh](int) { refresh(); });
        connect(rt, QOverload<int>::of(&QSpinBox::valueChanged), this,
                [refresh](int) { refresh(); });
        connect(st, QOverload<int>::of(&QSpinBox::valueChanged), this,
                [refresh](int) { refresh(); });
        connect(text, &QLineEdit::textChanged, this,
                [refresh](const QString&) { refresh(); });
    }

    // 清屏
    {
        auto* box = new QGroupBox(QStringLiteral("清屏参数"), tabs);
        auto* form = new QFormLayout(box);
        auto* row = new QSpinBox(box);
        row->setRange(0, 6);
        row->setValue(0);
        row->setSpecialValueText(QStringLiteral("0 (全屏)"));
        form->addRow(QStringLiteral("行号"), row);

        auto refresh = addCommandTab(tabs, QStringLiteral("清屏"), box,
            [row]() { return sc_etc::clearFrame(row->value()); });
        connect(row, QOverload<int>::of(&QSpinBox::valueChanged), this,
                [refresh](int) { refresh(); });
    }

    // 灯控
    {
        auto* box = new QGroupBox(QStringLiteral("灯控参数"), tabs);
        auto* form = new QFormLayout(box);
        auto* action = new QComboBox(box);
        action->addItem(QStringLiteral("交通灯红 (0A 36 0D)"), static_cast<int>(sc_etc::Light::Red));
        action->addItem(QStringLiteral("交通灯绿 (0A 37 0D)"), static_cast<int>(sc_etc::Light::Green));
        action->addItem(QStringLiteral("黄闪开 (0A 38 0D)"), static_cast<int>(sc_etc::Light::YellowOn));
        action->addItem(QStringLiteral("黄闪关 (0A 39 0D)"), static_cast<int>(sc_etc::Light::YellowOff));
        form->addRow(QStringLiteral("动作"), action);

        auto refresh = addCommandTab(tabs, QStringLiteral("灯控"), box,
            [action]() {
                return sc_etc::lightFrame(static_cast<sc_etc::Light>(action->currentData().toInt()));
            });
        connect(action, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [refresh](int) { refresh(); });
    }

    // 亮度
    {
        auto* box = new QGroupBox(QStringLiteral("亮度参数"), tabs);
        auto* form = new QFormLayout(box);
        auto* level = new QSpinBox(box);
        level->setRange(0, 7);
        level->setValue(7);
        level->setSpecialValueText(QStringLiteral("0 (自动调光)"));
        form->addRow(QStringLiteral("亮度等级"), level);

        auto refresh = addCommandTab(tabs, QStringLiteral("亮度"), box,
            [level]() { return sc_etc::brightnessFrame(static_cast<quint8>(level->value())); });
        connect(level, QOverload<int>::of(&QSpinBox::valueChanged), this,
                [refresh](int) { refresh(); });
    }

    // 心跳
    {
        auto* form = new QWidget(tabs);
        auto* lay = new QVBoxLayout(form);
        lay->addWidget(new QLabel(
            QStringLiteral("心跳保活帧，无参数。设备启用心跳机制后 5 分钟无有效帧\n"
                           "将自动显示「ETC车道关闭」，建议周期 1 分钟发送。"),
            form));
        lay->addStretch(1);
        addCommandTab(tabs, QStringLiteral("心跳"), form,
                      []() { return sc_etc::heartbeatFrame(); });
    }

    // 初始化（复位）
    {
        auto* form = new QWidget(tabs);
        auto* lay = new QVBoxLayout(form);
        auto* warn = new QLabel(
            QStringLiteral("警告：数据首字节 0x30 触发设备初始化（按固件实际行为执行复位）。"), form);
        warn->setStyleSheet(QStringLiteral("color: #F38BA8; font-weight: bold;"));
        lay->addWidget(warn);
        lay->addStretch(1);
        addCommandTab(tabs, QStringLiteral("初始化"), form,
                      []() { return sc_etc::initFrame(); });
    }

    return tabs;
}