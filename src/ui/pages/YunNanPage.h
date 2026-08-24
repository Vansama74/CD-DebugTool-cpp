#pragma once
#include "core/IProtocolPage.h"
#include "protocol/yunnan/YunNanProtocol.h"

#include <QByteArray>
#include <QString>

#include <functional>

class QPlainTextEdit;
class QTabWidget;
class QWidget;
class ConnectConfigPanel;
class SerialTransport;

// 云南费显协议测试页（云南LED费显P5，协议版本 YN_FX_P5_1.0）。
// 设备侧扩展：0x01 全屏点亮七色（01红~07白）、'8' 亮度 0x00 自动档 + '1'~'8' 手动档。
class YunNanPage : public IProtocolPage {
    Q_OBJECT
public:
    explicit YunNanPage(QWidget* parent = nullptr);
    ~YunNanPage() override;

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
    void onFrameReceived(const yunnan::Frame& frame);
    void showBareTextReply(const QByteArray& data);

    static QString frameToHex(const QByteArray& frame);
    static QString frameToAscii(const QByteArray& frame);
    static void appendMonitor(QPlainTextEdit* mon, const QString& arrow,
                              const QByteArray& bytes);

    ConnectConfigPanel* m_connectPanel = nullptr;
    SerialTransport* m_transport = nullptr; // no parent: lives in its own thread
    yunnan::FrameParser m_parser;

    QPlainTextEdit* m_rxMonitor = nullptr;
    QPlainTextEdit* m_txMonitor = nullptr;
};