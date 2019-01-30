#include "webuihandler.h"

#include <QApplication>
#include <QFile>
#include <QtWebEngineCore/QWebEngineUrlRequestJob>
#include <QtWebEngineCore/QWebEngineUrlScheme>

#define SCHEMENAME "webui"

const QByteArray WebUiHandler::schemeName  = QByteArrayLiteral(SCHEMENAME);
const QUrl       WebUiHandler::settingsUrl = QUrl(QStringLiteral(SCHEMENAME ":settings"));

WebUiHandler::WebUiHandler(QObject* parent)
    : QWebEngineUrlSchemeHandler(parent)
{
}

void WebUiHandler::requestStarted(QWebEngineUrlRequestJob* job)
{
    static const QUrl       webUiOrigin(QStringLiteral(SCHEMENAME ":"));
    static const QByteArray GET(QByteArrayLiteral("GET"));
    static const QByteArray POST(QByteArrayLiteral("POST"));

    QByteArray method    = job->requestMethod();
    QUrl       url       = job->requestUrl();
    QUrl       initiator = job->initiator();

    if (method == GET && url == settingsUrl)
    {
        QFile* file = new QFile(QStringLiteral(":/Resources/settings/index.html"), job);
        file->open(QIODevice::ReadOnly);
        job->reply(QByteArrayLiteral("text/html"), file);
    }
    else if (method == POST && url == settingsUrl && initiator == webUiOrigin)
    {
        job->fail(QWebEngineUrlRequestJob::RequestAborted);
        QApplication::exit();
    }
    else
    {
        job->fail(QWebEngineUrlRequestJob::UrlNotFound);
    }
}

// static
void WebUiHandler::registerUrlScheme()
{
    QWebEngineUrlScheme webUiScheme(schemeName);
    webUiScheme.setFlags(QWebEngineUrlScheme::SecureScheme |
                         QWebEngineUrlScheme::LocalScheme |
                         QWebEngineUrlScheme::LocalAccessAllowed);
    QWebEngineUrlScheme::registerScheme(webUiScheme);
}
