#pragma once
#include <QDialog>

class QComboBox;

class LoginDialog : public QDialog {
    Q_OBJECT
public:
    explicit LoginDialog(QWidget* parent = nullptr);
    QString getProtocol() const;

private:
    QComboBox* m_combo = nullptr;
};
