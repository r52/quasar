#include "applauncher.h"
#include "dataserver.h"
#include "quasar.h"
#include "runguard.h"
#include "widgetregistry.h"

#include <QSettings>
#include <QtWebEngineWidgets/QWebEngineProfile>
#include <QtWidgets/QApplication>

int main(int argc, char* argv[])
{
    RunGuard guard("quasar_app_key");
    if (!guard.tryToRun())
        return 0;

    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication a(argc, argv);
    a.setQuitOnLastWindowClosed(false);

    QApplication::setApplicationName("Quasar");
    QApplication::setOrganizationName("Quasar");
    QSettings::setDefaultFormat(QSettings::IniFormat);

    QWebEngineProfile::defaultProfile()->setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);

    // preload
    // Quasar takes ownership of these
    DataServer*     server   = new DataServer();
    WidgetRegistry* reg      = new WidgetRegistry();
    AppLauncher*    launcher = new AppLauncher(server, reg);

    Quasar w(server, reg, launcher);
    w.hide();

    return a.exec();
}
