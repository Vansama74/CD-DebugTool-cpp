#pragma once
#include <QHash>
#include <QWidget>
#include <QtGlobal>

class LedIndicator;
class QVBoxLayout;

// Front/back LED visualization: two columns, each with 4 LEDs (关闭/红/绿/转).
class DisplayWidget : public QWidget {
    Q_OBJECT
public:
    explicit DisplayWidget(QWidget* parent = nullptr);

    void updateState(quint8 front, quint8 back);
    void clear();

private:
    QHash<quint8, LedIndicator*> m_frontLeds;
    QHash<quint8, LedIndicator*> m_backLeds;
};
