#include "quasar.h"
#include <QtWidgets/QApplication>

static Quasar *instance = nullptr;

void msg_handler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    if (nullptr != instance)
    {
        instance->logMessage(type, context, msg);
    }
}

int main(int argc, char *argv[])
{
    qInstallMessageHandler(msg_handler);
    qSetMessagePattern("[%{time}] %{type} - %{message}");
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication a(argc, argv);
    a.setQuitOnLastWindowClosed(false);

    Quasar w(instance);
    w.hide();

    return a.exec();
}