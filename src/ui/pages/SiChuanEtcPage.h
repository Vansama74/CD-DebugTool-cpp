#pragma once
#include "ui/pages/SerialProtocolPage.h"
#include "protocol/sichuan_etc/SiChuanEtcProtocol.h"

class SiChuanEtcPage : public SerialProtocolPage {
    Q_OBJECT
public:
    explicit SiChuanEtcPage(QWidget* parent = nullptr);

    QString key() const override { return QStringLiteral("sichuan_etc"); }
    QString fullName() const override { return QStringLiteral("四川ETC费显协议"); }

protected:
    void onRxData(const QByteArray& data) override;

private:
    QWidget* buildCommandTabs();

    sc_etc::AckScanner m_ackScanner;
};