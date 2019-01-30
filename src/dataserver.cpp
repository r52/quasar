#include "dataserver.h"

#include "dataextension.h"
#include "widgetdefs.h"

#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QSettings>
#include <QtNetwork/QSslCertificate>
#include <QtNetwork/QSslKey>
#include <QtWebSockets/QWebSocket>
#include <QtWebSockets/QWebSocketServer>

#include <unordered_set>

#define DS_SEND_WARN(c, m)   \
    QString e(m);            \
    sendErrorToClient(c, e); \
    qWarning() << e;

std::unordered_set<QString> g_reservedcodes = {
    "global", "settings"
};

DataServer::DataServer(QObject* parent)
    : QObject(parent), m_pWebSocketServer(nullptr)
{
    m_pWebSocketServer = new QWebSocketServer(QStringLiteral("Data Server"),
                                              QWebSocketServer::SecureMode,
                                              this);

    QSslConfiguration sslConfiguration;
    QFile             certFile(QStringLiteral(":/Resources/localhost.crt"));
    QFile             keyFile(QStringLiteral(":/Resources/localhost.key"));
    certFile.open(QIODevice::ReadOnly);
    keyFile.open(QIODevice::ReadOnly);
    QSslCertificate certificate(&certFile, QSsl::Pem);
    QSslKey         sslKey(&keyFile, QSsl::Rsa, QSsl::Pem);
    certFile.close();
    keyFile.close();
    sslConfiguration.setPeerVerifyMode(QSslSocket::VerifyNone);
    sslConfiguration.setLocalCertificate(certificate);
    sslConfiguration.setPrivateKey(sslKey);
    sslConfiguration.setProtocol(QSsl::TlsV1SslV3);
    m_pWebSocketServer->setSslConfiguration(sslConfiguration);

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

        loadExtensions();
    }

    using namespace std::placeholders;
    m_methodmap["subscribe"] = std::bind(&DataServer::handleMethodSubscribe, this, _1, _2);
    m_methodmap["query"]     = std::bind(&DataServer::handleMethodQuery, this, _1, _2);
    m_methodmap["auth"]      = std::bind(&DataServer::handleMethodAuth, this, _1, _2);
}

DataServer::~DataServer()
{
    m_methodmap.clear();

    m_extensions.clear();

    m_authedclients.clear();

    m_authcodes.clear();

    m_pWebSocketServer->close();
}

bool DataServer::addMethodHandler(QString type, HandlerFuncType handler)
{
    if (m_methodmap.count(type))
    {
        qWarning() << "Handler for request type " << type << " already exists";
        return false;
    }

    m_methodmap[type] = handler;

    return true;
}

bool DataServer::findExtension(QString extcode)
{
    std::shared_lock<std::shared_mutex> lk(m_extmutex);
    return (m_extensions.count(extcode) > 0);
}

QString DataServer::generateAuthCode(QString ident)
{
    quint64 v  = QRandomGenerator::global()->generate64();
    QString hv = QString::number(v, 16).toUpper();

    std::unique_lock<std::mutex> lk(m_codemutex);
    m_authcodes.insert({ hv, { ident, CAL_WIDGET, system_clock::now() + 10s } });

    return hv;
}

void DataServer::loadExtensions()
{
    QDir          dir("extensions/");
    QFileInfoList list = dir.entryInfoList(QStringList() << "*.dll"
                                                         << "*.so"
                                                         << "*.dylib",
                                           QDir::Files);

    if (list.count() == 0)
    {
        qInfo() << "No data extensions found";
        return;
    }

    // just lock the whole thing while initializing extensions at startup
    // to prevent out of order reads
    std::unique_lock<std::shared_mutex> lk(m_extmutex);

    for (QFileInfo& file : list)
    {
        QString libpath = file.path() + "/" + file.fileName();

        qInfo() << "Loading data extension" << libpath;

        DataExtension* extn = DataExtension::load(libpath, this);

        if (!extn)
        {
            qWarning() << "Failed to load extension" << libpath;
        }
        else if (g_reservedcodes.count(extn->getCode()))
        {
            qWarning() << "The extension code" << extn->getCode() << " is reserved. Unloading " << libpath;
        }
        else if (m_extensions.count(extn->getCode()))
        {
            qWarning() << "Extension with code " << extn->getCode() << " already loaded. Unloading" << libpath;
        }
        else
        {
            qInfo() << "Extension " << extn->getCode() << " loaded.";
            m_extensions[extn->getCode()].reset(extn);
            extn = nullptr;
        }

        if (extn != nullptr)
        {
            delete extn;
        }
    }
}

void DataServer::handleRequest(const QJsonObject& req, QWebSocket* sender)
{
    if (req.isEmpty())
    {
        DS_SEND_WARN(sender, "Invalid JSON request");
        return;
    }

    QString mtd = req["method"].toString();

    if (!m_methodmap.count(mtd))
    {
        DS_SEND_WARN(sender, "Unknown method type " + mtd);
        return;
    }

    m_methodmap[mtd](req, sender);
}

void DataServer::handleMethodSubscribe(const QJsonObject& req, QWebSocket* sender)
{
    QString widgetName;

    {
        std::shared_lock<std::shared_mutex> lk(m_authmutex);

        auto it = m_authedclients.find(sender);
        if (it == m_authedclients.end())
        {
            DS_SEND_WARN(sender, "Unauthenticated client");
            return;
        }

        widgetName = it->second.ident;
    }

    auto parms = req["params"].toObject();

    if (parms.count() != 2)
    {
        DS_SEND_WARN(sender, "Invalid parameters for method 'subscribe'");
        return;
    }

    QString extcode = parms["target"].toString();
    QString extdata = parms["data"].toString();

    std::shared_lock<std::shared_mutex> lk(m_extmutex);

    if (!m_extensions.count(extcode))
    {
        DS_SEND_WARN(sender, "Unknown extension code " + extcode);
        return;
    }

    QStringList dlist = extdata.split(',', QString::SkipEmptyParts);

    for (QString& src : dlist)
    {
        if (m_extensions[extcode]->addSubscriber(src, sender, widgetName))
        {
            qInfo() << "Widget " << widgetName << " subscribed to extension " << extcode << " data " << src;
        }
        else
        {
            DS_SEND_WARN(sender, "Failed to subscribed to extension " + extcode + " data " + src);
        }
    }
}

void DataServer::handleMethodQuery(const QJsonObject& req, QWebSocket* sender)
{
    QString widgetName;

    {
        std::shared_lock<std::shared_mutex> lk(m_authmutex);

        auto it = m_authedclients.find(sender);
        if (it == m_authedclients.end())
        {
            DS_SEND_WARN(sender, "Unauthenticated client");
            return;
        }

        widgetName = it->second.ident;
    }

    auto parms = req["params"].toObject();

    if (parms.count() != 2)
    {
        DS_SEND_WARN(sender, "Invalid parameters for method 'query'");
        return;
    }

    QString extcode = parms["target"].toString();
    QString extdata = parms["data"].toString();

    std::shared_lock<std::shared_mutex> lk(m_extmutex);

    if (!m_extensions.count(extcode))
    {
        DS_SEND_WARN(sender, "Unknown extension code " + extcode);
        return;
    }

    // Add client to poll queue
    if (m_extensions[extcode]->addSubscriber(extdata, sender, widgetName))
    {
        m_extensions[extcode]->pollAndSendData(extdata, sender, widgetName);
    }
}

void DataServer::handleMethodAuth(const QJsonObject& req, QWebSocket* sender)
{
    {
        std::shared_lock<std::shared_mutex> lk(m_authmutex);
        if (m_authedclients.count(sender))
        {
            DS_SEND_WARN(sender, "Client already authenticated.");
            return;
        }
    }

    auto parms = req["params"].toObject();

    if (parms.count() != 1)
    {
        DS_SEND_WARN(sender, "Invalid parameters for method 'auth'");
        return;
    }

    QString authcode = parms["code"].toString();

    std::unique_lock<std::mutex> lk(m_codemutex);

    auto it = m_authcodes.find(authcode);
    if (it == m_authcodes.end())
    {
        DS_SEND_WARN(sender, "Invalid authentication code");
        return;
    }

    std::unique_lock<std::shared_mutex> lkm(m_authmutex);
    m_authedclients.insert({ sender, it->second });

    qInfo() << "Widget ident " << it->second.ident << " authenticated.";

    m_authcodes.erase(it);
}

void DataServer::checkAuth(QWebSocket* client)
{
    {
        // Check auth
        std::shared_lock<std::shared_mutex> lk(m_authmutex);
        if (!m_authedclients.count(client))
        {
            // unauthenticated client, cut the connection
            DS_SEND_WARN(client, "Unauthenticated client, disconnecting");
            client->close(QWebSocketProtocol::CloseCodeNormal, "Unauthenticated client");
        }
    }

    {
        // Clean expired auth codes
        std::unique_lock<std::mutex> lk(m_codemutex);
        for (auto it = m_authcodes.begin(); it != m_authcodes.end();)
        {
            if (it->second.expiry > system_clock::now())
            {
                it = m_authcodes.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }
}

void DataServer::sendErrorToClient(QWebSocket* client, QString err)
{
    // Craft error json msg
    QJsonObject msg;
    msg["error"] = err;

    QJsonDocument doc(msg);

    client->sendTextMessage(QString::fromUtf8(doc.toJson()));
}

void DataServer::onNewConnection()
{
    QWebSocket* pSocket = m_pWebSocketServer->nextPendingConnection();

    pSocket->setParent(this);
    connect(pSocket, &QWebSocket::textMessageReceived, this, &DataServer::processMessage);
    connect(pSocket, &QWebSocket::disconnected, this, &DataServer::socketDisconnected);
    QTimer::singleShot(10000, this, [this, pSocket] {
        checkAuth(pSocket);
    });
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
        {
            std::shared_lock<std::shared_mutex> lk(m_extmutex);
            for (auto& p : m_extensions)
            {
                p.second->removeSubscriber(pClient);
            }
        }

        {
            std::unique_lock<std::shared_mutex> lkm(m_authmutex);
            m_authedclients.erase(pClient);
        }

        pClient->deleteLater();
    }
}
