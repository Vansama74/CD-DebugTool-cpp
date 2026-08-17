#include "MainWindow.h"

#include <QCloseEvent>
#include <QComboBox>
#include <QLabel>
#include <QStackedWidget>
#include <QStatusBar>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include "config/ConfigManager.h"
#include "core/IProtocolPage.h"
#include "core/ProtocolRegistry.h"
#include "ui/widgets/LogPanel.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("CD DebugTool"));
    resize(1300, 780);

    auto* central = new QWidget(this);

    auto* topRow = new QHBoxLayout();
    auto* protoLabel = new QLabel(QStringLiteral("协议:"), central);
    m_combo = new QComboBox(central);
    m_combo->setObjectName(QStringLiteral("protocol_combo"));
    topRow->addWidget(protoLabel);
    topRow->addWidget(m_combo);
    topRow->addStretch(1);

    m_stack = new QStackedWidget(central);
    m_stack->setObjectName(QStringLiteral("content_stack"));

    const auto& protocols = ProtocolRegistry::instance().all();
    for (const ProtocolDescriptor& d : protocols) {
        IProtocolPage* page = d.factory(this);
        m_stack->addWidget(page);
        m_indexByKey.insert(d.key, m_stack->indexOf(page));
        m_combo->addItem(d.fullName, d.key);
    }

    m_logPanel = new LogPanel(central);
    m_logPanel->setMaximumHeight(200);
    for (int i = 0; i < m_stack->count(); ++i)
        static_cast<IProtocolPage*>(m_stack->widget(i))->setLogPanel(m_logPanel);

    auto* root = new QVBoxLayout(central);
    root->addLayout(topRow);
    root->addWidget(m_stack, 1);
    root->addWidget(m_logPanel);

    setCentralWidget(central);
    statusBar()->showMessage(QStringLiteral("就绪"));

    connect(m_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int idx) {
                const QString key = m_combo->itemData(idx).toString();
                if (!key.isEmpty())
                    switchToMode(key);
            });

    // 启动时恢复：窗口几何 + 上次使用的协议页（预选，登录对话框默认项同步）。
    const QJsonObject cfg = ConfigManager::load();
    const QString geom = ConfigManager::getString(cfg, "window_geometry", QString());
    if (!geom.isEmpty()) {
        const QByteArray g = QByteArray::fromBase64(geom.toLatin1());
        if (!g.isEmpty())
            restoreGeometry(g);
    }
    const int lastTab = ConfigManager::getInt(cfg, "last_tab", 0);
    if (lastTab >= 0 && lastTab < m_combo->count()) {
        const QSignalBlocker blocker(m_combo);
        m_combo->setCurrentIndex(lastTab);
    }
}

void MainWindow::switchToMode(const QString& key)
{
    auto it = m_indexByKey.constFind(key);
    if (it == m_indexByKey.constEnd())
        return;

    if (m_currentPage && m_currentPage->key() == key)
        return;

    const int idx = it.value();

    if (m_currentPage)
        m_currentPage->deactivate();

    m_stack->setCurrentIndex(idx);
    m_currentPage = static_cast<IProtocolPage*>(m_stack->widget(idx));
    if (m_currentPage)
        m_currentPage->activate();

    const int comboIdx = m_combo->findData(key);
    if (comboIdx >= 0 && m_combo->currentIndex() != comboIdx)
        m_combo->setCurrentIndex(comboIdx);

    if (m_currentPage)
        setWindowTitle(QStringLiteral("CD DebugTool — ") + m_currentPage->fullName());
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    // 退出时保存窗口几何与当前协议页。
    QJsonObject cfg = ConfigManager::load();
    cfg.insert(QStringLiteral("window_geometry"),
               QString::fromLatin1(saveGeometry().toBase64()));
    if (m_currentPage) {
        const int idx = m_combo->findData(m_currentPage->key());
        cfg.insert(QStringLiteral("last_tab"), idx >= 0 ? idx : 0);
    }
    ConfigManager::save(cfg);
    QMainWindow::closeEvent(event);
}
