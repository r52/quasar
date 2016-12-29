#include "quasar.h"
#include <QtWidgets/QApplication>
#include <QFileInfo>

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication a(argc, argv);
    a.setQuitOnLastWindowClosed(false);

    Quasar w;
    w.hide();

    return a.exec();
}
