#pragma once
#include <QString>
#include <QVector>
#include <QWidget>

class QComboBox;
class QPlainTextEdit;

// LogPanel: shared colored log widget (bottom bar) reused by every protocol page.
class LogPanel : public QWidget {
    Q_OBJECT
public:
    explicit LogPanel(QWidget* parent = nullptr);

    void append(const QString& message, const QString& level = QStringLiteral("INFO"));
    void clear();

private:
    struct Entry {
        QString level;
        QString message;
        QString timestamp;
    };

    static QString colorForLevel(const QString& level);
    bool passesFilter(const QString& level) const;
    void renderLine(const Entry& e);
    void renderAll();

    QPlainTextEdit* m_logText = nullptr;
    QComboBox* m_levelCombo = nullptr;
    QVector<Entry> m_entries;
};
