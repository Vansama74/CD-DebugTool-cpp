#pragma once
#include "core/IProtocolPage.h"

#include <QByteArray>

class Rs485ControlPanel;
class SerialTransport;

class Rs485Page : public IProtocolPage {
    Q_OBJECT
public:
    explicit Rs485Page(QWidget* parent = nullptr);
    ~Rs485Page() override;

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
    void onFrameReceived(const QByteArray& frame);

    Rs485ControlPanel* m_controlPanel = nullptr;
    SerialTransport* m_transport = nullptr; // no parent: lives in its own thread
    QByteArray m_rxBuffer;
};
