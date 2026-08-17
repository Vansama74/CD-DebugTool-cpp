#include "ThemeManager.h"

#include <QFile>
#include <QProcess>
#include <QStringList>

ThemeManager::Theme ThemeManager::detect()
{
    QProcess proc;
    proc.start(QStringLiteral("gsettings"),
               QStringList() << QStringLiteral("get")
                             << QStringLiteral("org.gnome.desktop.interface")
                             << QStringLiteral("color-scheme"));
    if (proc.waitForFinished(1500)
        && proc.exitStatus() == QProcess::NormalExit
        && proc.exitCode() == 0) {
        const QByteArray out = proc.readAllStandardOutput();
        if (out.toLower().contains("dark"))
            return Theme::Dark;
    }
    return Theme::Light;
}

QString ThemeManager::loadQss(Theme t)
{
    const QString path = (t == Theme::Dark)
        ? QStringLiteral(":/theme/dark.qss")
        : QStringLiteral(":/theme/light.qss");

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    return QString::fromUtf8(f.readAll());
}
