#include "LoginDialog.h"

#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "config/ConfigManager.h"
#include "core/ProtocolRegistry.h"

LoginDialog::LoginDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("CD DebugTool — 选择协议"));
    setMinimumSize(320, 180);

    auto* title = new QLabel(QStringLiteral("选择协议模式"), this);
    title->setObjectName(QStringLiteral("titleLabel"));
    title->setAlignment(Qt::AlignCenter);

    m_combo = new QComboBox(this);
    m_combo->setObjectName(QStringLiteral("protocol_combo"));

    const auto& protocols = ProtocolRegistry::instance().all();
    for (const ProtocolDescriptor& d : protocols)
        m_combo->addItem(d.fullName, d.key);

    // 默认预选上次使用的协议页（MainWindow 关闭时保存的 last_tab）。
    const QJsonObject cfg = ConfigManager::load();
    const int lastTab = ConfigManager::getInt(cfg, "last_tab", 0);
    if (lastTab >= 0 && lastTab < m_combo->count())
        m_combo->setCurrentIndex(lastTab);

    auto* okBtn = new QPushButton(QStringLiteral("确定"), this);
    okBtn->setObjectName(QStringLiteral("primaryBtn"));
    okBtn->setDefault(true);
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(title);
    layout->addWidget(m_combo);
    layout->addWidget(okBtn);
    layout->addStretch(1);
}

QString LoginDialog::getProtocol() const
{
    return m_combo->currentData().toString();
}
