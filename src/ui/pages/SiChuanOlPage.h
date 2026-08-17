#pragma once
#include "ui/pages/SerialProtocolPage.h"
#include "protocol/sichuan_ol/SiChuanOlProtocol.h"

class SiChuanOlPage : public SerialProtocolPage {
    Q_OBJECT
public:
    explicit SiChuanOlPage(QWidget* parent = nullptr);

    QString key() const override { return QStringLiteral("sichuan_ol"); }
    QString fullName() const override { return QStringLiteral("四川治超屏协议"); }

protected:
    void onRxData(const QByteArray& data) override;

private:
    QWidget* buildCommandTabs();

    sc_ol::FrameParser m_parser;
};