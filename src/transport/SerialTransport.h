#pragma once
#include <QByteArray>
#include <QMutex>
#include <QObject>
#include <QQueue>
#include <QString>
#include <QStringList>
#include <QThread>

class QSerialPort;

// SerialTransport: runs a QSerialPort inside a dedicated QThread.
//
// QSerialPort is thread-affine: it must be created and used in the thread it
// lives in. This class satisfies that by moving itself to a private QThread in
// the constructor and doing all serial I/O inside worker-thread slots:
//
//   * onThreadStarted() (runs in the worker thread after start()) creates the
//     QSerialPort and wires readyRead/errorOccurred.
//   * open()/close()/send() are safe to call from any thread (e.g. the GUI);
//     they marshal the actual work into the worker thread via
//     QMetaObject::invokeMethod(..., Qt::QueuedConnection).
//   * send() stages outgoing bytes in a QMutex-guarded queue, drained by the
//     drainSendQueue() slot, so callers never block on the serial write.
class SerialTransport : public QObject {
    Q_OBJECT
public:
    explicit SerialTransport(QObject* parent = nullptr);
    ~SerialTransport() override;

    void start();
    void stop();

    void open(const QString& port, int baud);
    void close();
    void send(const QByteArray& data);

    static QStringList availablePorts();
    static QString portLabel(const QString& systemLocation);

signals:
    void bytesReceived(const QByteArray& data);
    void connected(const QString& port, int baud);
    void disconnected();
    void errorOccurred(const QString& msg);

private slots:
    void onThreadStarted();
    void doOpen(const QString& port, int baud);
    void doClose();
    void drainSendQueue();

private:
    QThread m_thread;
    QMutex m_sendMutex;
    QQueue<QByteArray> m_sendQueue;
    QSerialPort* m_serial = nullptr;
    // 仅 worker 线程访问：记录“端口曾成功打开”，用于丢弃数据提示的节流
    // （只在打开→未打开状态转换后的第一次丢弃时发一次错误）。
    bool m_portWasOpen = false;
};
