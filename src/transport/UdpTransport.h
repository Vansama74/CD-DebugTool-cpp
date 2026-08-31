#pragma once
#include <QByteArray>
#include <QObject>
#include <QString>
#include <QThread>

class QUdpSocket;

// UdpTransport: runs a QUdpSocket inside a dedicated QThread (same
// thread-affinity pattern as SerialTransport — see its header for details).
// M2 scope is transport-only: raw bytes out, raw bytes in; no IAP framing.
class UdpTransport : public QObject {
    Q_OBJECT
public:
    explicit UdpTransport(QObject* parent = nullptr);
    ~UdpTransport() override;

    void start();
    void stop();
    void bind(quint16 port, const QString& bindIp = QString());
    void sendBroadcast(const QByteArray& data, quint16 port);
    void sendUnicast(const QByteArray& data, const QString& ip, quint16 port);
    // 底层 socket 是否已开启 SO_BROADCAST（诊断/测试用；跨线程阻塞式查询）。
    bool isBroadcastEnabled();

signals:
    void frameReceived(const QByteArray& data, const QString& srcIp, quint16 srcPort);
    void errorOccurred(const QString& msg);

private slots:
    void onThreadStarted();
    void doBind(quint16 port, const QString& bindIp);
    void doShutdown();
    void doSendBroadcast(const QByteArray& data, quint16 port);
    void doSendUnicast(const QByteArray& data, const QString& ip, quint16 port);

private:
    QThread m_thread;
    QUdpSocket* m_socket = nullptr;
    quint16 m_port = 0;
    QString m_bindIp;
    bool m_bindRequested = false;
};
