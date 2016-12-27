#include "quasar.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    Quasar w;
    w.show();
    return a.exec();
}
