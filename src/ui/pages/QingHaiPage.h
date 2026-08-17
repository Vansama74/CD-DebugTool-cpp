#pragma once
#include "core/IProtocolPage.h"
#include "protocol/qinghai/QingHaiProtocol.h"

#include <QByteArray>
#include <QString>

#include <functional>

class QPlainTextEdit;
class QTabWidget;
class QWidget;
class ConnectConfigPanel;
class SerialTransport;

class QingHaiPage : public IProtocolPage {
    Q_OBJECT
public:
    explicit QingHaiPage(QWidget* parent = nullptr);
    ~QingHaiPage() override;

    QString key() const override;
    QString fullName() const override;
    void activate() override;
    void deactivate() override;

private slots:
    void onPortOpened(const QString& port, int baud);
    void onPortClosed();
    void onTransportConnected(const QString& port, int baud);
    void onTransportDisconnected();
    void onTransportError(const QString& msg);
    void onBytesReceived(const QByteArray& data);

private:
    QWidget* buildCommandTabs();
    QWidget* buildMonitorTab();
    std::function<void()> addCommandTab(QTabWidget* tabs, const QString& title,
                                        QWidget* form,
                                        std::function<QByteArray()> builder);
    void onSendCommand(const std::function<QByteArray()>& builder);
    void onCopyCommand(const std::function<QByteArray()>& builder);
    void onFrameReceived(const qinghai::Frame& frame);

    static QString frameToHex(const QByteArray& frame);

    ConnectConfigPanel* m_connectPanel = nullptr;
    SerialTransport* m_transport = nullptr; // no parent: lives in its own thread
    qinghai::FrameParser m_parser;

    QPlainTextEdit* m_rxMonitor = nullptr;
    QPlainTextEdit* m_txMonitor = nullptr;
};
