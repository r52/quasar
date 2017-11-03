#include "dataserver.h"

#include "dataplugin.h"
#include "quasar.h"
#include "webwidget.h"
#include "widgetdefs.h"
#include "widgetregistry.h"

#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QtWebSockets/QWebSocket>
#include <QtWebSockets/QWebSocketServer>

DataServer::DataServer(QObject* parent)
    : QObject(parent), m_pWebSocketServer(nullptr)
{
    m_pWebSocketServer = new QWebSocketServer(QStringLiteral("Data Server"),
                                              QWebSocketServer::NonSecureMode,
                                              this);

    QSettings settings;
    quint16   port = settings.value(QUASAR_CONFIG_PORT, QUASAR_DATA_SERVER_DEFAULT_PORT).toUInt();

    if (!m_pWebSocketServer->listen(QHostAddress::LocalHost, port))
    {
        qWarning() << "Data server failed to bind port" << port;
    }
    else
    {
        qInfo() << "Data server running locally on port" << port;
        connect(m_pWebSocketServer, &QWebSocketServer::newConnection, this, &DataServer::onNewConnection);

        loadDataPlugins();
    }

    using namespace std::placeholders;
    m_reqcallmap["subscribe"] = std::bind(&DataServer::handleSubscribeReq, this, _1, _2);
    m_reqcallmap["poll"]      = std::bind(&DataServer::handlePollReq, this, _1, _2);
}

DataServer::~DataServer()
{
    m_reqcallmap.clear();

    m_plugins.clear();

    m_pWebSocketServer->close();
}

bool DataServer::addHandler(QString type, HandlerFuncType handler)
{
    if (m_reqcallmap.count(type))
    {
        qWarning() << "Handler for request type " << type << " already exists";
        return false;
    }

    m_reqcallmap[type] = handler;

    return true;
}

void DataServer::loadDataPlugins()
{
    QDir          dir("plugins/");
    QFileInfoList list = dir.entryInfoList(QStringList() << "*.dll"
                                                         << "*.so"
                                                         << "*.dylib",
                                           QDir::Files);

    if (list.count() == 0)
    {
        qInfo() << "No data plugins found";
        return;
    }

    for (QFileInfo& file : list)
    {
        QString libpath = file.path() + "/" + file.fileName();

        qInfo() << "Loading data plugin" << libpath;

        DataPlugin* plugin = DataPlugin::load(libpath, this);

        if (!plugin)
        {
            qWarning() << "Failed to load plugin" << libpath;
        }
        else if (plugin->getCode() == "global")
        {
            qWarning() << "The 'global' plugin code is reserved. Unloading" << libpath;
        }
        else if (m_plugins.count(plugin->getCode()))
        {
            qWarning() << "Plugin with code " << plugin->getCode() << " already loaded. Unloading" << libpath;
        }
        else
        {
            qInfo() << "Plugin " << plugin->getCode() << " loaded.";
            m_plugins[plugin->getCode()].reset(plugin);
            plugin = nullptr;
        }

        if (plugin != nullptr)
        {
            delete plugin;
        }
    }
}

void DataServer::handleRequest(const QJsonObject& req, QWebSocket* sender)
{
    if (req.isEmpty())
    {
        qWarning() << "Invalid JSON object request";
        return;
    }

    QString type = req["type"].toString();

    if (!m_reqcallmap.count(type))
    {
        qWarning() << "Unknown request type";
        return;
    }

    m_reqcallmap[type](req, sender);
}

void DataServer::handleSubscribeReq(const QJsonObject& req, QWebSocket* sender)
{
    QString widgetName = req["widget"].toString();
    QString plugin     = req["plugin"].toString();
    QString sources    = req["source"].toString();

    if (!m_plugins.count(plugin))
    {
        qWarning() << "Unknown plugin " << plugin;
        return;
    }

    QStringList srclist = sources.split(',', QString::SkipEmptyParts);

    for (QString& src : srclist)
    {
        if (m_plugins[plugin]->addSubscriber(src, sender, widgetName))
        {
            qInfo() << "Widget " << widgetName << " subscribed to plugin " << plugin << " source " << src;
        }
        else
        {
            qWarning() << "Widget " << widgetName << " failed to subscribed to plugin " << plugin << " source " << src;
        }
    }
}

void DataServer::handlePollReq(const QJsonObject& req, QWebSocket* sender)
{
    QString widgetName = req["widget"].toString();
    QString plugin     = req["plugin"].toString();
    QString source     = req["source"].toString();

    if (!m_plugins.count(plugin))
    {
        qWarning() << "Unknown plugin " << plugin;
        return;
    }

    // Add client to poll queue
    if (m_plugins[plugin]->addSubscriber(source, sender, widgetName))
    {
        m_plugins[plugin]->pollAndSendData(source, sender, widgetName);
    }
}

void DataServer::onNewConnection()
{
    QWebSocket* pSocket = m_pWebSocketServer->nextPendingConnection();

    pSocket->setParent(this);
    connect(pSocket, &QWebSocket::textMessageReceived, this, &DataServer::processMessage);
    connect(pSocket, &QWebSocket::disconnected, this, &DataServer::socketDisconnected);
}

void DataServer::processMessage(QString message)
{
    QWebSocket* pSender = qobject_cast<QWebSocket*>(sender());

    if (pSender)
    {
        QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());

        if (doc.isNull())
        {
            qWarning() << "Error parsing WebSocket message";
            qWarning() << message;
            return;
        }

        handleRequest(doc.object(), pSender);
    }
}

void DataServer::socketDisconnected()
{
    QWebSocket* pClient = qobject_cast<QWebSocket*>(sender());
    if (pClient)
    {
        for (auto& p : m_plugins)
        {
            p.second->removeSubscriber(pClient);
        }

        pClient->deleteLater();
    }
}
