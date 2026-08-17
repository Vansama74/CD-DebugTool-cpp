#include "SiChuanOlPage.h"

#include "protocol/common/Codec.h"
#include "ui/widgets/LogPanel.h"

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

SiChuanOlPage::SiChuanOlPage(QWidget* parent)
    : SerialProtocolPage(9600, parent) // 治超协议波特率 9600
{
    auto* cmdTabs = buildCommandTabs();
    setupTabs(cmdTabs);
}

void SiChuanOlPage::onRxData(const QByteArray& data)
{
    m_parser.feed(data);
    sc_ol::Frame frame;
    while (m_parser.next(&frame)) {
        if (!m_log)
            continue;
        if (!frame.bccOk) {
            m_log->append(QStringLiteral("治超应答帧 BCC 校验失败"), QStringLiteral("WARN"));
            continue;
        }

        const sc_ol::ContentReply content = sc_ol::parseContentReply(frame);
        if (content.ok) {
            m_log->append(QStringLiteral("行 %1 显示: %2")
                              .arg(content.row + 1)
                              .arg(cd::fromGbk(content.text)),
                          QStringLiteral("SUCCESS"));
            continue;
        }

        const sc_ol::StatusReply status = sc_ol::parseStatusReply(frame);
        if (status.ok) {
            switch (frame.cmd) {
            case 0xB6:
                m_log->append(QStringLiteral("当前亮度: %1 (00 最暗 / FF 最亮)")
                                  .arg(status.value, 2, 16, QLatin1Char('0'))
                                  .toUpper(),
                              QStringLiteral("SUCCESS"));
                break;
            case 0xB9:
                m_log->append(status.value != 0 ? QStringLiteral("当前通行灯: 绿")
                                                : QStringLiteral("当前通行灯: 红"),
                              QStringLiteral("SUCCESS"));
                break;
            case 0xB8:
                m_log->append(status.value != 0 ? QStringLiteral("当前黄闪: 开")
                                                : QStringLiteral("当前黄闪: 关"),
                              QStringLiteral("SUCCESS"));
                break;
            default:
                break;
            }
            continue;
        }

        m_log->append(QStringLiteral("未知治超应答帧 (命令 0x%1)")
                          .arg(frame.cmd, 2, 16, QLatin1Char('0'))
                          .toUpper(),
                      QStringLiteral("WARN"));
    }
}

QWidget* SiChuanOlPage::buildCommandTabs()
{
    auto* tabs = new QTabWidget(this);

    // 全屏显示
    {
        auto* box = new QGroupBox(QStringLiteral("全屏显示参数"), tabs);
        auto* form = new QFormLayout(box);
        auto* text = new QLineEdit(box);
        text->setPlaceholderText(QStringLiteral("全屏文本（GBK，≤12 汉字）"));
        text->setText(QStringLiteral("四川治超检测"));
        form->addRow(QStringLiteral("文本"), text);

        auto refresh = addCommandTab(tabs, QStringLiteral("全屏显示"), box,
            [text]() { return sc_ol::fullScreenFrame(text->text()); });
        connect(text, &QLineEdit::textChanged, this,
                [refresh](const QString&) { refresh(); });
    }

    // 行显示
    {
        auto* box = new QGroupBox(QStringLiteral("行显示参数"), tabs);
        auto* form = new QFormLayout(box);
        auto* row = new QSpinBox(box);
        row->setRange(1, sc_ol::kLineCount);
        row->setValue(1);
        auto* text = new QLineEdit(box);
        text->setPlaceholderText(QStringLiteral("行文本（GBK，≤8 汉字，不足补空格）"));
        text->setText(QStringLiteral("车牌：川A12345"));
        form->addRow(QStringLiteral("行号"), row);
        form->addRow(QStringLiteral("文本"), text);

        auto refresh = addCommandTab(tabs, QStringLiteral("行显示"), box,
            [row, text]() { return sc_ol::lineFrame(row->value() - 1, text->text()); });
        connect(row, QOverload<int>::of(&QSpinBox::valueChanged), this,
                [refresh](int) { refresh(); });
        connect(text, &QLineEdit::textChanged, this,
                [refresh](const QString&) { refresh(); });
    }

    // 清屏
    {
        auto* form = new QWidget(tabs);
        auto* lay = new QVBoxLayout(form);
        lay->addWidget(new QLabel(QStringLiteral("清空整屏显示。无参数。"), form));
        lay->addStretch(1);
        addCommandTab(tabs, QStringLiteral("清屏"), form,
                      []() { return sc_ol::clearFrame(); });
    }

    // 亮度
    {
        auto* box = new QGroupBox(QStringLiteral("亮度调节"), tabs);
        auto* form = new QFormLayout(box);
        auto* val = new QSpinBox(box);
        val->setRange(0, 255);
        val->setValue(255);
        val->setSpecialValueText(QStringLiteral("00 (自动调光)"));
        form->addRow(QStringLiteral("亮度值"), val);

        auto refresh = addCommandTab(tabs, QStringLiteral("亮度"), box,
            [val]() { return sc_ol::brightnessFrame(static_cast<quint8>(val->value())); });
        connect(val, QOverload<int>::of(&QSpinBox::valueChanged), this,
                [refresh](int) { refresh(); });
    }

    // 通行灯
    {
        auto* box = new QGroupBox(QStringLiteral("通行灯控制"), tabs);
        auto* form = new QFormLayout(box);
        auto* state = new QComboBox(box);
        state->addItem(QStringLiteral("红 (00)"), false);
        state->addItem(QStringLiteral("绿 (01)"), true);
        state->setCurrentIndex(1);
        form->addRow(QStringLiteral("通行灯"), state);

        auto refresh = addCommandTab(tabs, QStringLiteral("通行灯"), box,
            [state]() { return sc_ol::laneLightFrame(state->currentData().toBool()); });
        connect(state, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [refresh](int) { refresh(); });
    }

    // 黄闪
    {
        auto* box = new QGroupBox(QStringLiteral("黄闪控制"), tabs);
        auto* form = new QFormLayout(box);
        auto* state = new QComboBox(box);
        state->addItem(QStringLiteral("关 (00)"), false);
        state->addItem(QStringLiteral("开 (01)"), true);
        state->setCurrentIndex(0);
        form->addRow(QStringLiteral("黄闪"), state);

        auto refresh = addCommandTab(tabs, QStringLiteral("黄闪"), box,
            [state]() { return sc_ol::yellowFlashFrame(state->currentData().toBool()); });
        connect(state, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [refresh](int) { refresh(); });
    }

    // 查询显示内容
    {
        auto* form = new QWidget(tabs);
        auto* lay = new QVBoxLayout(form);
        lay->addWidget(new QLabel(
            QStringLiteral("设备逐行应答 A1~A8 帧（每行 16B 数据 + 当前亮度）。"), form));
        lay->addStretch(1);
        addCommandTab(tabs, QStringLiteral("查询显示内容"), form,
                      []() { return sc_ol::queryContentFrame(); });
    }

    // 查询亮度
    {
        auto* form = new QWidget(tabs);
        auto* lay = new QVBoxLayout(form);
        lay->addWidget(new QLabel(QStringLiteral("设备应答 B6 帧（当前亮度值 00~FF）。"), form));
        lay->addStretch(1);
        addCommandTab(tabs, QStringLiteral("查询亮度"), form,
                      []() { return sc_ol::queryBrightFrame(); });
    }

    // 查询通行灯
    {
        auto* form = new QWidget(tabs);
        auto* lay = new QVBoxLayout(form);
        lay->addWidget(new QLabel(QStringLiteral("设备应答 B9 帧（00 红 / 01 绿）。"), form));
        lay->addStretch(1);
        addCommandTab(tabs, QStringLiteral("查询通行灯"), form,
                      []() { return sc_ol::queryLaneFrame(); });
    }

    // 查询黄闪
    {
        auto* form = new QWidget(tabs);
        auto* lay = new QVBoxLayout(form);
        lay->addWidget(new QLabel(QStringLiteral("设备应答 B8 帧（00 关 / 01 开）。"), form));
        lay->addStretch(1);
        addCommandTab(tabs, QStringLiteral("查询黄闪"), form,
                      []() { return sc_ol::queryFlashFrame(); });
    }

    return tabs;
}