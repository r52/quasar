#include "dataserver.h"
#include "dataplugin.h"
#include "quasar.h"

#include <QDebug>
#include <QDir>
#include <QSettings>
#include <QtWebSockets/QWebSocketServer>
#include <QtWebSockets/QWebSocket>
#include <QJsonDocument>
#include <QJsonObject>

DataServer::DataServer(Quasar *parent) :
    m_parent(parent),
    QObject((QObject*)parent),
    m_pWebSocketServer(nullptr)
{
    if (nullptr == m_parent)
    {
        throw std::invalid_argument("Parent cannot be null");
    }

    m_pWebSocketServer = new QWebSocketServer(QStringLiteral("Data Server"),
        QWebSocketServer::NonSecureMode,
        this);

    QSettings settings;
    quint16 port = settings.value("global/dataport", 13337).toUInt();

    if (m_pWebSocketServer->listen(QHostAddress::LocalHost, port))
    {
        qInfo() << "Data server running locally on port" << port;
        connect(m_pWebSocketServer, &QWebSocketServer::newConnection,
            this, &DataServer::onNewConnection);

        loadDataPlugins();
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

void DataServer::loadDataPlugins()
{
    QDir dir("plugins/");
    QFileInfoList list = dir.entryInfoList(QDir::Files);

    if (list.count() == 0)
    {
        qInfo() << "No data plugins found";
        return;
    }

    for (QFileInfo &file : list)
    {
        QString libpath = file.path() + "/" + file.completeBaseName();

        qInfo() << "Loading data plugin" << libpath;

        DataPlugin* plugin = DataPlugin::load(libpath, this);

        if (!plugin)
        {
            qInfo() << "Failed to load plugin" << libpath;
        }
        else if (!plugin->setupPlugin())
        {
            qInfo() << "Failed to setup plugin" << libpath;
        }
        else if (m_plugins.contains(plugin->getCode()))
        {
            qWarning() << "Plugin with code '" << plugin->getCode() << "' already loaded. Unloading" << libpath;
        }
        else
        {
            m_plugins[plugin->getCode()] = plugin;
            plugin = nullptr;
        }

        if (plugin != nullptr)
        {
            delete plugin;
        }
    }
}

void DataServer::handleRequest(const QJsonObject &req, QWebSocket *sender)
{
    if (req.isEmpty())
    {
        qWarning() << "Invalid JSON object request from '" << sender->peerName() << "'";
        return;
    }

    QString type = req["type"].toString();

    if (type == "subscribe")
    {
        QString widgetName = req["widget"].toString();
        QString plugin = req["plugin"].toString();
        QString source = req["source"].toString();

        // subWidget parameter currently unused
        WebWidget *subWidget = m_parent->getWidgetRegistry().findWidgetByName(widgetName);

        if (!subWidget)
        {
            qWarning() << "Unidentified widget name '" << widgetName << "'";
            return;
        }

        if (!m_plugins.contains(plugin))
        {
            qWarning() << "Unknown plugin '" << plugin << "'";
            return;
        }

        m_plugins[plugin]->addSubscriber(source, sender);
    }
    else
    {
        qWarning() << "Unknown request type";
    }
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

    if (pSender)
    {
        QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());

        if (doc.isNull())
        {
            qWarning() << "Error parsing message from '" << pSender->peerName() << "'";
            qWarning() << message;
            return;
        }

        handleRequest(doc.object(), pSender);
    }
}

void DataServer::socketDisconnected()
{
    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    if (pClient)
    {
        m_clients.removeAll(pClient);

        for (DataPlugin *plugin : m_plugins)
        {
            plugin->removeSubscriber(pClient);
        }

        pClient->deleteLater();
    }
}