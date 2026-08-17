#pragma once
#include <QWidget>
#include <QString>

class LogPanel;
class IProtocolPage : public QWidget {
    Q_OBJECT
public:
    explicit IProtocolPage(QWidget* parent = nullptr) : QWidget(parent) {}
    ~IProtocolPage() override = default;
    virtual QString key() const = 0;        // "qinghai" | "iap" | "rs485"
    virtual QString fullName() const = 0;   // full Chinese name
    virtual void activate() {}
    virtual void deactivate() {}
    void setLogPanel(LogPanel* log) { m_log = log; }
    LogPanel* logPanel() const { return m_log; }
protected:
    LogPanel* m_log = nullptr;
};
