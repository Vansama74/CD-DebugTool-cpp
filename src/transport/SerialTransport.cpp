#include "SerialTransport.h"

#include <QIODevice>
#include <QList>
#include <QMetaObject>
#include <QMutexLocker>
#include <QSerialPort>
#include <QSerialPortInfo>

SerialTransport::SerialTransport(QObject* parent)
    : QObject(parent)
{
    moveToThread(&m_thread);
    connect(&m_thread, &QThread::started, this, &SerialTransport::onThreadStarted);
}

SerialTransport::~SerialTransport()
{
    stop();
}

void SerialTransport::start()
{
    if (!m_thread.isRunning())
        m_thread.start();
}

void SerialTransport::stop()
{
    if (m_thread.isRunning()) {
        QMetaObject::invokeMethod(this, "doClose", Qt::QueuedConnection);
        m_thread.quit();
        m_thread.wait();
    }
}

void SerialTransport::open(const QString& port, int baud)
{
    QMetaObject::invokeMethod(this, "doOpen", Qt::QueuedConnection,
                              Q_ARG(QString, port), Q_ARG(int, baud));
}

void SerialTransport::close()
{
    QMetaObject::invokeMethod(this, "doClose", Qt::QueuedConnection);
}

void SerialTransport::send(const QByteArray& data)
{
    if (data.isEmpty())
        return;
    {
        QMutexLocker locker(&m_sendMutex);
        m_sendQueue.enqueue(data);
    }
    QMetaObject::invokeMethod(this, "drainSendQueue", Qt::QueuedConnection);
}

void SerialTransport::onThreadStarted()
{
    if (m_serial)
        return;

    m_serial = new QSerialPort(this);

    connect(m_serial, &QSerialPort::readyRead, this, [this]() {
        const QByteArray data = m_serial->readAll();
        if (!data.isEmpty())
            emit bytesReceived(data);
    });

    connect(m_serial,
            QOverload<QSerialPort::SerialPortError>::of(&QSerialPort::errorOccurred),
            this, [this](QSerialPort::SerialPortError err) {
        if (err == QSerialPort::NoError)
            return;
        const QString msg = m_serial->errorString();
        m_serial->clearError();
        emit errorOccurred(msg);
        // 端口被拔出（ResourceError）或已不再打开时，同步真实状态：
        // 通知上层“已断开”，让连接面板回写 UI，避免假连接。
        if (err == QSerialPort::ResourceError || !m_serial->isOpen()) {
            m_portWasOpen = false;
            emit disconnected();
        }
    });
}

void SerialTransport::doOpen(const QString& port, int baud)
{
    if (!m_serial) {
        emit errorOccurred(QStringLiteral("串口未初始化"));
        return;
    }

    if (m_serial->isOpen())
        m_serial->close();

    m_serial->setPortName(port);
    m_serial->setBaudRate(baud);
    m_serial->setDataBits(QSerialPort::Data8);
    m_serial->setParity(QSerialPort::NoParity);
    m_serial->setStopBits(QSerialPort::OneStop);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);

    if (!m_serial->open(QIODevice::ReadWrite)) {
        emit errorOccurred(QStringLiteral("打开串口 %1 失败: %2")
                               .arg(port, m_serial->errorString()));
        return;
    }

    m_portWasOpen = true;
    emit connected(port, baud);
}

void SerialTransport::doClose()
{
    if (m_serial && m_serial->isOpen()) {
        m_serial->close();
        m_portWasOpen = false;
        emit disconnected();
    }
}

void SerialTransport::drainSendQueue()
{
    if (!m_serial || !m_serial->isOpen()) {
        QList<QByteArray> dropped;
        {
            QMutexLocker locker(&m_sendMutex);
            while (!m_sendQueue.isEmpty())
                dropped.append(m_sendQueue.dequeue());
        }
        // 节流：仅在“曾打开过”之后首次丢弃数据时提示一次，避免高频刷屏。
        if (m_portWasOpen && !dropped.isEmpty()) {
            m_portWasOpen = false;
            emit errorOccurred(QStringLiteral("串口未打开，发送数据已丢弃"));
        }
        return;
    }

    QList<QByteArray> batch;
    {
        QMutexLocker locker(&m_sendMutex);
        while (!m_sendQueue.isEmpty())
            batch.append(m_sendQueue.dequeue());
    }
    for (const QByteArray& frame : batch)
        m_serial->write(frame);
}

QStringList SerialTransport::availablePorts()
{
    QStringList result;
    const QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo& info : ports) {
        const QString loc = info.systemLocation();
#ifdef Q_OS_WIN
        // On Windows systemLocation() already returns the long form
        // (e.g. \\.\COM10), which QSerialPort::setPortName() accepts directly.
        // Every listed port is a real COM port, so no filtering is needed.
        if (loc.isEmpty())
            continue;
#else
        const bool isUsb = loc.contains(QStringLiteral("/dev/ttyUSB"))
                        || loc.contains(QStringLiteral("/dev/ttyACM"))
                        || loc.contains(QStringLiteral("/dev/ttyAMA"));
        const bool isNative = loc.contains(QStringLiteral("/dev/ttyS"));
        if (!isUsb && !isNative)
            continue;

        // ttyS* can be present-but-unusable; probe-open to confirm.
        if (isNative) {
            QSerialPort probe;
            probe.setPortName(info.portName());
            if (!probe.open(QIODevice::ReadOnly))
                continue;
            probe.close();
        }
#endif
        result.append(loc);
    }
    return result;
}

QString SerialTransport::portLabel(const QString& systemLocation)
{
    const QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo& info : ports) {
        if (info.systemLocation() != systemLocation)
            continue;
        QString label = systemLocation;
        if (!info.description().isEmpty())
            label += QStringLiteral(" (%1)").arg(info.description());
        return label;
    }
    return systemLocation;
}
