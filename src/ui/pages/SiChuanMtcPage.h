#pragma once
#include "ui/pages/SerialProtocolPage.h"
#include "protocol/sichuan_mtc/SiChuanMtcProtocol.h"

class QTimer;

class SiChuanMtcPage : public SerialProtocolPage {
    Q_OBJECT
public:
    explicit SiChuanMtcPage(QWidget* parent = nullptr);

    QString key() const override { return QStringLiteral("sichuan_mtc"); }
    QString fullName() const override { return QStringLiteral("四川MTC费显协议"); }

protected:
    void onRxData(const QByteArray& data) override;

private:
    void handleReply(const sc_mtc::Reply& reply);

    QWidget* buildCommandTabs();

    sc_mtc::ReplyScanner m_replyScanner;
    QTimer* m_flushTimer = nullptr; // 无封套文本应答（版本号）静默超时冲刷
};