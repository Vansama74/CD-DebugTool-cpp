#pragma once
#include <QColor>
#include <QString>
#include <QWidget>

// A single LED indicator: a colored circle plus a label, painted with QPainter.
class LedIndicator : public QWidget {
    Q_OBJECT
public:
    explicit LedIndicator(const QString& text = QString(),
                          const QColor& color = QColor(QStringLiteral("#30363D")),
                          int size = 20, QWidget* parent = nullptr);

    void setColor(const QColor& color);
    void setText(const QString& text);
    void setActive(bool active);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QString m_text;
    QColor m_color;
    int m_size;
    bool m_active = false;
};
