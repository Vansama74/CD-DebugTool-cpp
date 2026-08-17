#pragma once
#include "ui/pages/SerialProtocolPage.h"
#include "protocol/shandong/ShanDongProtocol.h"

class QTimer;

class ShanDongPage : public SerialProtocolPage {
    Q_OBJECT
public:
    explicit ShanDongPage(QWidget* parent = nullptr);

    QString key() const override { return QStringLiteral("shandong"); }
    QString fullName() const override { return QStringLiteral("山东车道费显协议"); }

protected:
    void onRxData(const QByteArray& data) override;

private:
    void handleReply(const shandong::Reply& reply);

    QWidget* buildCommandTabs();

    shandong::ReplyScanner m_replyScanner;
    QTimer* m_flushTimer = nullptr; // 无封套文本应答（版本号）静默超时冲刷
};