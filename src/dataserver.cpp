#include "dataserver.h"

#include <QSettings>
#include <QtWebSockets/QWebSocketServer>
#include <QtWebSockets/QWebSocket>

DataServer::DataServer(QObject *parent) : 
    QObject(parent),
    m_pWebSocketServer(nullptr)
{
    m_pWebSocketServer = new QWebSocketServer(QStringLiteral("Data Server"),
        QWebSocketServer::NonSecureMode,
        this);

    QSettings settings;
    quint16 port = settings.value("global/dataport", 13337).toUInt();

    if (m_pWebSocketServer->listen(QHostAddress::LocalHost, port))
    {
        qDebug() << "Data server running locally on port" << port;
        connect(m_pWebSocketServer, &QWebSocketServer::newConnection,
            this, &DataServer::onNewConnection);
    }
    else
    {
        qWarning() << "Data server failed to bind port" << port;
    }
}

DataServer::~DataServer()
{
    m_pWebSocketServer->close();
    qDeleteAll(m_clients.begin(), m_clients.end());
}

void DataServer::onNewConnection()
{
    QWebSocket *pSocket = m_pWebSocketServer->nextPendingConnection();

    connect(pSocket, &QWebSocket::textMessageReceived, this, &DataServer::processMessage);
    connect(pSocket, &QWebSocket::disconnected, this, &DataServer::socketDisconnected);

    m_clients << pSocket;
}

void DataServer::processMessage(QString message)
{
    QWebSocket *pSender = qobject_cast<QWebSocket *>(sender());

    // TODO: code this
}

void DataServer::socketDisconnected()
{
    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    if (pClient)
    {
        m_clients.removeAll(pClient);

        // TODO: also delete from plugins
        pClient->deleteLater();
    }
}
