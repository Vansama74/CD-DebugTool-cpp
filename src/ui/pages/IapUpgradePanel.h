#pragma once
#include <QWidget>

class QJsonObject;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;

// Center panel: firmware selection + upgrade control + progress bar.
class IapUpgradePanel : public QWidget {
    Q_OBJECT
public:
    explicit IapUpgradePanel(QWidget* parent = nullptr);

    void updateFirmwareInfo(const QJsonObject& info);
    void setUpgrading(bool upgrading);
    void updateProgress(double percent, int current, int total);
    void updateStatus(const QString& text);

signals:
    void firmwareSelected(const QString& path);
    void startUpgrade();
    void cancelUpgrade();
    void pauseUpgrade();
    void resumeUpgrade();
    void rebootRequested();
    void recoveryRequested();

private slots:
    void browseFile();
    void onStart();
    void onCancel();
    void onPauseResume();

private:
    QString m_firmwarePath;
    bool m_upgrading = false;

    QLineEdit* m_pathEdit = nullptr;
    QLabel* m_sizeLabel = nullptr;
    QLabel* m_crcLabel = nullptr;
    QLabel* m_packetsLabel = nullptr;
    QProgressBar* m_progress = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_pktLabel = nullptr;
    QPushButton* m_startBtn = nullptr;
    QPushButton* m_cancelBtn = nullptr;
    QPushButton* m_pauseBtn = nullptr;
    QPushButton* m_rebootBtn = nullptr;
    QPushButton* m_recoveryBtn = nullptr;
};
