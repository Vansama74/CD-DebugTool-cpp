#include "LogPanel.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTime>
#include <QVBoxLayout>

LogPanel::LogPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(6, 2, 6, 4);
    root->setSpacing(2);

    auto* ctrl = new QHBoxLayout();

    auto* label = new QLabel(QStringLiteral("活动日志"), this);
    label->setObjectName(QStringLiteral("subtitleLabel"));
    ctrl->addWidget(label);
    ctrl->addStretch(1);

    m_levelCombo = new QComboBox(this);
    m_levelCombo->addItems({QStringLiteral("全部"), QStringLiteral("信息"),
                            QStringLiteral("警告"), QStringLiteral("错误")});
    m_levelCombo->setFixedWidth(80);
    ctrl->addWidget(m_levelCombo);

    auto* clearBtn = new QPushButton(QStringLiteral("清除"), this);
    clearBtn->setMinimumWidth(64);
    clearBtn->setCursor(Qt::PointingHandCursor);
    connect(clearBtn, &QPushButton::clicked, this, &LogPanel::clear);
    ctrl->addWidget(clearBtn);

    root->addLayout(ctrl);

    m_logText = new QPlainTextEdit(this);
    m_logText->setReadOnly(true);
    m_logText->setMaximumBlockCount(5000);
    m_logText->setMinimumHeight(80);
    root->addWidget(m_logText);

    connect(m_levelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { renderAll(); });
}

void LogPanel::append(const QString& message, const QString& level)
{
    Entry e;
    e.level = level;
    e.message = message;
    e.timestamp = QTime::currentTime().toString(QStringLiteral("HH:mm:ss"));

    m_entries.append(e);
    if (m_entries.size() > 5000)
        m_entries.removeFirst();

    if (passesFilter(level))
        renderLine(e);
}

void LogPanel::clear()
{
    m_entries.clear();
    m_logText->clear();
}

QString LogPanel::colorForLevel(const QString& level)
{
    if (level == QStringLiteral("WARN"))    return QStringLiteral("#F9E2AF");
    if (level == QStringLiteral("ERROR"))   return QStringLiteral("#F38BA8");
    if (level == QStringLiteral("SUCCESS")) return QStringLiteral("#A6E3A1");
    if (level == QStringLiteral("CMD"))     return QStringLiteral("#89B4FA");
    return QStringLiteral("#A6ADC8"); // INFO (and unknown) fallback
}

bool LogPanel::passesFilter(const QString& level) const
{
    switch (m_levelCombo->currentIndex()) {
    case 1:  return level != QStringLiteral("WARN") && level != QStringLiteral("ERROR"); // 信息
    case 2:  return level == QStringLiteral("WARN");                                     // 警告
    case 3:  return level == QStringLiteral("ERROR");                                    // 错误
    default: return true;                                                                 // 全部
    }
}

void LogPanel::renderLine(const Entry& e)
{
    const QString line = QStringLiteral("<span style=\"color:#6C7086\">[%1]</span> "
                                        "<span style=\"color:%2\">%3</span>")
                             .arg(e.timestamp.toHtmlEscaped(),
                                  colorForLevel(e.level),
                                  e.message.toHtmlEscaped());
    m_logText->appendHtml(line);

    QScrollBar* sb = m_logText->verticalScrollBar();
    sb->setValue(sb->maximum());
}

void LogPanel::renderAll()
{
    m_logText->clear();
    for (const Entry& e : m_entries) {
        if (passesFilter(e.level))
            renderLine(e);
    }
}
