#pragma once
#include <QWidget>

class QByteArray;
class QCheckBox;
class QLineEdit;
class QPushButton;

// Hex send/receive panel: raw hex input with optional auto-checksum.
class HexDumpPanel : public QWidget {
    Q_OBJECT
public:
    explicit HexDumpPanel(QWidget* parent = nullptr);

    // Shows the latest response in the input placeholder.
    void appendRx(const QString& hexLine);

signals:
    void sendHex(const QByteArray& data);

private slots:
    void onSend();

private:
    QLineEdit* m_hexInput = nullptr;
    QCheckBox* m_autoChecksum = nullptr;
    QPushButton* m_sendBtn = nullptr;
};
