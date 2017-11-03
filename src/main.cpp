#include "applauncher.h"
#include "dataserver.h"
#include "logwindow.h"
#include "quasar.h"
#include "runguard.h"
#include "widgetdefs.h"
#include "widgetregistry.h"

#include <QSettings>
#include <QSplashScreen>
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

    QPixmap       pixmap(":/Resources/splash.png");
    QSplashScreen splash(pixmap);
    splash.show();

    splash.showMessage("Loading configuration...", Qt::AlignHCenter | Qt::AlignBottom, Qt::white);
    a.processEvents();

    QApplication::setApplicationName("Quasar");
    QApplication::setOrganizationName("Quasar");
    QSettings::setDefaultFormat(QSettings::IniFormat);

    QWebEngineProfile::defaultProfile()->setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);

    splash.showMessage("Loading modules...", Qt::AlignHCenter | Qt::AlignBottom, Qt::white);
    a.processEvents();

    // preload
    // Quasar takes ownership of these
    LogWindow*      log      = new LogWindow();
    DataServer*     server   = new DataServer();
    WidgetRegistry* reg      = new WidgetRegistry();
    AppLauncher*    launcher = new AppLauncher(server, reg);

    Quasar w(log, server, reg, launcher);
    w.hide();

    splash.showMessage("Loading widgets...", Qt::AlignHCenter | Qt::AlignBottom, Qt::white);
    a.processEvents();

    // Load widgets
    QSettings   settings;
    QStringList loadedList = settings.value(QUASAR_CONFIG_LOADED).toStringList();

    for (const QString& f : loadedList)
    {
        reg->loadWebWidget(f, false);
    }

    splash.finish(&w);

    return a.exec();
}
