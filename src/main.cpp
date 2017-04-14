#include "quasar.h"
#include "widgetdefs.h"

#include <plugin_types.h>

#include <QtWidgets/QApplication>
#include <QSettings>
#include <QTextEdit>

static QTextEdit* logEdit = nullptr;

void msg_handler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    if (nullptr != logEdit)
    {
        QSettings setting;
        int loglevel = setting.value(QUASAR_CONFIG_LOGLEVEL, QUASAR_LOG_INFO).toInt();
        bool print = false;

        switch (type)
        {
            case QtDebugMsg:
                print = (loglevel == QUASAR_LOG_DEBUG);
                break;

            case QtInfoMsg:
                print = (loglevel <= QUASAR_LOG_INFO);
                break;

            case QtWarningMsg:
                print = (loglevel <= QUASAR_LOG_WARNING);
                break;

            case QtCriticalMsg:
                print = (loglevel <= QUASAR_LOG_CRITICAL);
                break;

            default:
                print = true;
                break;
        }

        if (print)
        {
            logEdit->append(qFormatLogMessage(type, context, msg));
        }
    }
}

int main(int argc, char *argv[])
{
    qInstallMessageHandler(msg_handler);
    qSetMessagePattern("[%{time}] - %{type} - %{function} - %{message}");

    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication a(argc, argv);
    a.setQuitOnLastWindowClosed(false);

    QApplication::setApplicationName("Quasar");
    QApplication::setOrganizationName("Quasar");
    QSettings::setDefaultFormat(QSettings::IniFormat);

    logEdit = new QTextEdit();
    logEdit->setReadOnly(true);
    logEdit->setAcceptRichText(true);

    Quasar w(logEdit);
    w.hide();

    return a.exec();
}
