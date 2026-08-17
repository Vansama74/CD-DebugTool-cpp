#pragma once
#include "core/IProtocolPage.h"

#include <QByteArray>
#include <QString>

#include <functional>

class QPlainTextEdit;
class QTabWidget;
class QTimer;
class QWidget;
class ConnectConfigPanel;
class SerialTransport;

// 串口协议页基类：封装 ConnectConfigPanel + SerialTransport 生命周期、
// TX/RX 监视页与「协议帧生成」标签页框架（命令表单 + 帧预览 + 发送/复制）。
//
// 子类只需：构造内层命令标签页（通过 addCommandTab），调用 setupTabs()，
// 并重写 onRxData() 解析设备应答。
class SerialProtocolPage : public IProtocolPage {
    Q_OBJECT
public:
    explicit SerialProtocolPage(int defaultBaud, QWidget* parent = nullptr);
    ~SerialProtocolPage() override;

    void activate() override;
    void deactivate() override;

    // 设置“发送后无应答”超时毫秒数（默认 2000，<=0 忽略）。
    void setReplyTimeoutMs(int ms);

protected:
    // 组装外层标签页 [串口 / 协议帧生成 / 监视]，需在子类构造末尾调用。
    void setupTabs(QWidget* commandTabs);

    // 生成一个命令标签页，返回刷新函数（表单值变化时调用以更新帧预览）。
    // expectReply=false：单向命令（协议规定设备不应答），发送后不启动
    // 「无应答」超时定时器。
    std::function<void()> addCommandTab(QTabWidget* tabs, const QString& title,
                                        QWidget* form,
                                        std::function<QByteArray()> builder,
                                        bool expectReply = true);

    // 发送帧（未打开串口时告警并返回 false）；复制帧到剪贴板。
    // expectReply=false：发送后不启动「无应答」超时定时器，
    // 并在日志追加中性确认「命令已发送（协议无应答）」。
    bool sendFrame(const QByteArray& frame, bool expectReply = true);
    void copyFrame(const QByteArray& frame);

    // 原始接收字节 → 监视页 + 日志。
    void appendRx(const QByteArray& data);
    // 子类解析钩子（默认无操作）。
    virtual void onRxData(const QByteArray& data);

    ConnectConfigPanel* connectPanel() const { return m_connectPanel; }
    static QString frameToHex(const QByteArray& frame);

private slots:
    void onPortOpened(const QString& port, int baud);
    void onPortClosed();
    void onTransportConnected(const QString& port, int baud);
    void onTransportDisconnected();
    void onTransportError(const QString& msg);
    void onBytesReceived(const QByteArray& data);

private:
    ConnectConfigPanel* m_connectPanel = nullptr;
    SerialTransport* m_transport = nullptr; // 无父对象：驻留自身线程
    QPlainTextEdit* m_rxMonitor = nullptr;
    QPlainTextEdit* m_txMonitor = nullptr;
    // 单次“无应答”超时定时器：sendFrame 成功后重启，收到任意数据时停止。
    QTimer* m_replyTimer = nullptr;
    int m_replyTimeoutMs = 2000;
};