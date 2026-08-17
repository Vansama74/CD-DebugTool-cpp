#include "ShanDongPage.h"

#include "ui/widgets/LogPanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

ShanDongPage::ShanDongPage(QWidget* parent)
    : SerialProtocolPage(115200, parent) // 山东协议默认波特率 115200
{
    auto* cmdTabs = buildCommandTabs();
    setupTabs(cmdTabs);

    // 版本号等无封套文本应答可能在一次串口突发内完整到达（无终止字节），
    // 静默 300ms 后冲刷扫描器缓冲兜底。
    m_flushTimer = new QTimer(this);
    m_flushTimer->setSingleShot(true);
    m_flushTimer->setInterval(300);
    connect(m_flushTimer, &QTimer::timeout, this, [this]() {
        shandong::Reply reply;
        while (m_replyScanner.flush(&reply))
            handleReply(reply);
    });
}

void ShanDongPage::onRxData(const QByteArray& data)
{
    m_replyScanner.feed(data);
    shandong::Reply reply;
    while (m_replyScanner.next(&reply))
        handleReply(reply);
    if (m_flushTimer)
        m_flushTimer->start();
}

void ShanDongPage::handleReply(const shandong::Reply& reply)
{
    if (!reply.valid || !m_log)
        return;
    m_log->append(QStringLiteral("设备返回版本号: %1").arg(reply.text),
                  QStringLiteral("SUCCESS"));
}

QWidget* ShanDongPage::buildCommandTabs()
{
    auto* tabs = new QTabWidget(this);

    // 全屏单色（'1'）
    {
        auto* box = new QGroupBox(QStringLiteral("全屏单色参数"), tabs);
        auto* form = new QFormLayout(box);
        auto* color = new QComboBox(box);
        color->addItem(QStringLiteral("红 (0x01)"), 1);
        color->addItem(QStringLiteral("绿 (0x02)"), 2);
        color->addItem(QStringLiteral("黄 (0x03)"), 3);
        color->setCurrentIndex(0);
        form->addRow(QStringLiteral("颜色"), color);

        auto refresh = addCommandTab(tabs, QStringLiteral("全屏单色"), box,
            [color]() { return shandong::fillAllFrame(color->currentData().toInt()); },
            /*expectReply=*/false);
        connect(color, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [refresh](int) { refresh(); });
    }

    // 取版本号（'2'）
    {
        auto* form = new QWidget(tabs);
        auto* lay = new QVBoxLayout(form);
        lay->addWidget(new QLabel(
            QStringLiteral("查询设备版本号。设备以裸 ASCII 应答产品程序编码\n"
                           "PROGRAM_CODE（如 9K10212482），结果显示在下方日志。"),
            form));
        lay->addStretch(1);
        addCommandTab(tabs, QStringLiteral("取版本号"), form,
                      []() { return shandong::versionFrame(); });
    }

    // 单行显示（'3'）
    {
        auto* box = new QGroupBox(QStringLiteral("单行显示参数"), tabs);
        auto* form = new QFormLayout(box);
        auto* color = new QComboBox(box);
        color->addItem(QStringLiteral("0 红"), 0);
        color->addItem(QStringLiteral("1 绿"), 1);
        color->addItem(QStringLiteral("2 黄"), 2);
        auto* row = new QSpinBox(box);
        row->setRange(1, 5);
        row->setValue(1);
        auto* text = new QLineEdit(box);
        text->setPlaceholderText(QStringLiteral("单行文本（GBK 编码）"));
        text->setText(QStringLiteral("山东欢迎您"));
        form->addRow(QStringLiteral("颜色"), color);
        form->addRow(QStringLiteral("行号"), row);
        form->addRow(QStringLiteral("文本"), text);

        auto refresh = addCommandTab(tabs, QStringLiteral("单行显示"), box,
            [color, row, text]() {
                return shandong::oneLineFrame(color->currentData().toInt(),
                                              row->value(), text->text());
            },
            /*expectReply=*/false);
        connect(color, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [refresh](int) { refresh(); });
        connect(row, QOverload<int>::of(&QSpinBox::valueChanged), this,
                [refresh](int) { refresh(); });
        connect(text, &QLineEdit::textChanged, this,
                [refresh](const QString&) { refresh(); });
    }

    // 全屏可编辑（'4'）
    {
        auto* box = new QGroupBox(QStringLiteral("全屏可编辑参数"), tabs);
        auto* form = new QFormLayout(box);
        auto* color = new QComboBox(box);
        color->addItem(QStringLiteral("0 红"), 0);
        color->addItem(QStringLiteral("1 绿"), 1);
        color->addItem(QStringLiteral("2 黄"), 2);
        auto* x = new QSpinBox(box);
        x->setRange(0, 255);
        x->setValue(0);
        auto* y = new QSpinBox(box);
        y->setRange(0, 255);
        y->setValue(0);
        auto* text = new QPlainTextEdit(box);
        text->setPlaceholderText(QStringLiteral("全屏文本（GBK 编码，换行以 0x0A 发送）"));
        text->setPlainText(QStringLiteral("山东省高速公路欢迎您"));
        text->setFixedHeight(90);
        form->addRow(QStringLiteral("颜色"), color);
        form->addRow(QStringLiteral("X 坐标"), x);
        form->addRow(QStringLiteral("Y 坐标"), y);
        form->addRow(QStringLiteral("文本"), text);

        auto refresh = addCommandTab(tabs, QStringLiteral("全屏可编辑"), box,
            [color, x, y, text]() {
                return shandong::fullScreenFrame(color->currentData().toInt(),
                                                 static_cast<quint8>(x->value()),
                                                 static_cast<quint8>(y->value()),
                                                 text->toPlainText());
            },
            /*expectReply=*/false);
        connect(color, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [refresh](int) { refresh(); });
        connect(x, QOverload<int>::of(&QSpinBox::valueChanged), this,
                [refresh](int) { refresh(); });
        connect(y, QOverload<int>::of(&QSpinBox::valueChanged), this,
                [refresh](int) { refresh(); });
        connect(text, &QPlainTextEdit::textChanged, this, [refresh]() { refresh(); });
    }

    // 清屏（'5'）
    {
        auto* form = new QWidget(tabs);
        auto* lay = new QVBoxLayout(form);
        lay->addWidget(new QLabel(QStringLiteral("全屏清除显示。无参数。"), form));
        lay->addStretch(1);
        addCommandTab(tabs, QStringLiteral("清屏"), form,
                      []() { return shandong::clearFrame(); },
                      /*expectReply=*/false);
    }

    // 亮度（'7'）
    {
        auto* box = new QGroupBox(QStringLiteral("亮度设定"), tabs);
        auto* form = new QFormLayout(box);
        auto* level = new QSpinBox(box);
        level->setRange(0, 5);
        level->setValue(5);
        level->setSpecialValueText(QStringLiteral("0 (自动调节)"));
        form->addRow(QStringLiteral("亮度级别"), level);

        auto refresh = addCommandTab(tabs, QStringLiteral("亮度"), box,
            [level]() { return shandong::brightnessFrame(level->value()); },
            /*expectReply=*/false);
        connect(level, QOverload<int>::of(&QSpinBox::valueChanged), this,
                [refresh](int) { refresh(); });
    }

    // 外设（'8'）
    {
        auto* box = new QGroupBox(QStringLiteral("外设控制（位图）"), tabs);
        auto* form = new QFormLayout(box);
        auto* green = new QCheckBox(QStringLiteral("绿灯 (bit0)"), box);
        auto* red = new QCheckBox(QStringLiteral("红灯 (bit1)"), box);
        auto* yellow = new QCheckBox(QStringLiteral("黄闪报警 (bit2)"), box);
        green->setChecked(true);
        form->addRow(green);
        form->addRow(red);
        form->addRow(yellow);

        auto refresh = addCommandTab(tabs, QStringLiteral("外设"), box,
            [green, red, yellow]() {
                return shandong::peripheralFrame(green->isChecked(),
                                                 red->isChecked(),
                                                 yellow->isChecked());
            },
            /*expectReply=*/false);
        connect(green, &QCheckBox::toggled, this, [refresh](bool) { refresh(); });
        connect(red, &QCheckBox::toggled, this, [refresh](bool) { refresh(); });
        connect(yellow, &QCheckBox::toggled, this, [refresh](bool) { refresh(); });
    }

    return tabs;
}