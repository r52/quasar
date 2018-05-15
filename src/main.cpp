#include "dataservices.h"
#include "logwindow.h"
#include "quasar.h"
#include "runguard.h"
#include "widgetdefs.h"
#include "widgetregistry.h"

#include <QSettings>
#include <QSplashScreen>
#include <QThread>
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

    LogWindow* log = new LogWindow();

    QPixmap       pixmap(":/Resources/splash.png");
    QSplashScreen splash(pixmap);
    auto          align = Qt::AlignHCenter | Qt::AlignBottom;
    auto          color = Qt::white;
    splash.show();

    splash.showMessage("Loading configuration...", align, color);
    a.processEvents();

    QApplication::setApplicationName("quasar");
    QApplication::setOrganizationName("quasar");
    QSettings::setDefaultFormat(QSettings::IniFormat);

    QWebEngineProfile::defaultProfile()->setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);

    splash.showMessage("Loading services...", align, color);
    a.processEvents();

    // preload
    // Quasar takes ownership
    DataServices* service = new DataServices();

    Quasar w(log, service);
    w.hide();

    splash.showMessage("Loading widgets...", align, color);
    a.processEvents();

    // Load widgets
    QSettings   settings;
    QStringList loadedList = settings.value(QUASAR_CONFIG_LOADED).toStringList();

    for (const QString& f : loadedList)
    {
        service->getRegistry()->loadWebWidget(f, false);
    }

    splash.finish(&w);

    return a.exec();
}
