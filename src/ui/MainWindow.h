#pragma once
#include <QMainWindow>
#include <QHash>

class QComboBox;
class QCloseEvent;
class QStackedWidget;
class IProtocolPage;
class LogPanel;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

public slots:
    void switchToMode(const QString& key);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    QComboBox* m_combo = nullptr;
    QStackedWidget* m_stack = nullptr;
    QHash<QString, int> m_indexByKey;
    IProtocolPage* m_currentPage = nullptr;
    LogPanel* m_logPanel = nullptr;
};
