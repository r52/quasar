#include "logwindow.h"

#include "widgetdefs.h"
#include <plugin_types.h>

#include <QSettings>
#include <QTextEdit>

#include <mutex>

/*
* This is a fake singleton unique pointer hybrid monster thing
* designed to work around the fact that Qt widget parents take
* ownership of added child widgets, so that the lifetime of
* the logedit widget can be maximized and ensure proper clean up
*/

namespace
{
    std::mutex s_logMutex;
    QTextEdit* s_logEdit = nullptr;
}

void msg_handler(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    if (nullptr != s_logEdit)
    {
        QSettings setting;
        int       loglevel = setting.value(QUASAR_CONFIG_LOGLEVEL, QUASAR_LOG_INFO).toInt();
        bool      print    = false;

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
            std::unique_lock<std::mutex> lock(s_logMutex);
            s_logEdit->append(qFormatLogMessage(type, context, msg));
        }
    }
}

LogWindow::~LogWindow()
{
    // if released, s_logEdit is not owned anymore
    // otherwise it needs to be cleaned
    if (!m_released && nullptr != s_logEdit)
    {
        delete s_logEdit;
    }

    s_logEdit = nullptr;
}

LogWindow::LogWindow(QObject* parent)
    : QObject(parent)
{
    if (nullptr == parent)
    {
        throw std::invalid_argument("null parent");
    }

    if (nullptr != s_logEdit)
    {
        throw std::exception("log window already created");
    }

    s_logEdit = new QTextEdit();

    s_logEdit->setReadOnly(true);
    s_logEdit->setAcceptRichText(true);

    // max lines in log viewer
    s_logEdit->document()->setMaximumBlockCount(250);

    qInstallMessageHandler(msg_handler);
    qSetMessagePattern("[%{time}] - %{type} - %{function} - %{message}");
}

QTextEdit* LogWindow::release()
{
    if (nullptr != s_logEdit)
    {
        m_released = true;
    }

    return s_logEdit;
}
