#include "quasar.h"

#include "common/update.h"

#include <QApplication>
#include <QSettings>
#include <QSplashScreen>

int main(int argc, char* argv[])
{
    if (Update::GetUpdateStatus() == Update::HasUpdate)
    {
        Update::RunUpdate();
        return 0;
    }

    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    // Set settings type/path
    QCoreApplication::setOrganizationName("quasar");
    QCoreApplication::setApplicationName("quasar");
    QSettings::setDefaultFormat(QSettings::IniFormat);

    QApplication a(argc, argv);
    a.setQuitOnLastWindowClosed(false);

    QPixmap       pixmap(":/Resources/splash.png");
    QSplashScreen splash(pixmap);
    const auto    align = Qt::AlignHCenter | Qt::AlignBottom;
    const auto    color = Qt::white;
    splash.show();

    splash.showMessage("Loading...", align, color);

    Quasar w;
    w.hide();

    splash.finish(&w);

    return a.exec();
}
