#include "quasar.h"

#include <QtWidgets/QApplication>
#include <QSettings>
#include <QTextEdit>

static QTextEdit* logEdit = nullptr;

void msg_handler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    if (nullptr != logEdit)
    {
        logEdit->append(qFormatLogMessage(type, context, msg));
    }
}

int main(int argc, char *argv[])
{
    qInstallMessageHandler(msg_handler);
    qSetMessagePattern("[%{time}] %{type} - %{message}");

    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication a(argc, argv);
    a.setQuitOnLastWindowClosed(false);

    logEdit = new QTextEdit();
    logEdit->setReadOnly(true);
    logEdit->setAcceptRichText(true);

    QApplication::setApplicationName("Quasar");
    QApplication::setOrganizationName("Quasar");
    QSettings::setDefaultFormat(QSettings::IniFormat);

    Quasar w(logEdit);
    w.hide();

    return a.exec();
}