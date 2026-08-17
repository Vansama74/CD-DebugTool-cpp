#include <QApplication>
#include <QCoreApplication>
#include <QFont>

#include "core/ProtocolRegistry.h"
#include "core/ThemeManager.h"
#include "ui/LoginDialog.h"
#include "ui/MainWindow.h"
#include "ui/pages/QingHaiPage.h"
#include "ui/pages/IapPage.h"
#include "ui/pages/Rs485Page.h"
#include "ui/pages/SiChuanEtcPage.h"
#include "ui/pages/SiChuanMtcPage.h"
#include "ui/pages/SiChuanOlPage.h"
#include "ui/pages/ShanDongPage.h"

int main(int argc, char* argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("CD DebugTool"));
    app.setApplicationVersion(QStringLiteral("1.0.0"));
    app.setOrganizationName(QStringLiteral("ChuangDi"));

    app.setFont(QFont(QStringLiteral("Noto Sans CJK SC"), 9));

    ThemeManager::Theme t = ThemeManager::detect();
    app.setStyleSheet(ThemeManager::loadQss(t));

    auto& reg = ProtocolRegistry::instance();
    reg.registerProtocol({ QStringLiteral("qinghai"), QStringLiteral("青海高速费显协议"),
                           [](QWidget* p) { return new QingHaiPage(p); } });
    reg.registerProtocol({ QStringLiteral("iap"), QStringLiteral("IAP 远程升级"),
                           [](QWidget* p) { return new IapPage(p); } });
    reg.registerProtocol({ QStringLiteral("rs485"), QStringLiteral("重庆创迪车道指示器"),
                           [](QWidget* p) { return new Rs485Page(p); } });
    reg.registerProtocol({ QStringLiteral("sichuan_etc"), QStringLiteral("四川ETC费显协议"),
                           [](QWidget* p) { return new SiChuanEtcPage(p); } });
    reg.registerProtocol({ QStringLiteral("sichuan_mtc"), QStringLiteral("四川MTC费显协议"),
                           [](QWidget* p) { return new SiChuanMtcPage(p); } });
    reg.registerProtocol({ QStringLiteral("sichuan_ol"), QStringLiteral("四川治超屏协议"),
                           [](QWidget* p) { return new SiChuanOlPage(p); } });
    reg.registerProtocol({ QStringLiteral("shandong"), QStringLiteral("山东车道费显协议"),
                           [](QWidget* p) { return new ShanDongPage(p); } });

    MainWindow window;
    LoginDialog dlg;
    if (dlg.exec() != QDialog::Accepted)
        return 0;

    window.switchToMode(dlg.getProtocol());
    window.show();

    return app.exec();
}
