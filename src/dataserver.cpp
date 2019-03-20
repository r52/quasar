#include "dataserver.h"

#include "dataextension.h"
#include "extension_support_internal.h"
#include "widgetdefs.h"

#include <QDesktopServices>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRandomGenerator>
#include <QSettings>
#include <QStandardPaths>
#include <QtNetwork/QSslCertificate>
#include <QtNetwork/QSslKey>
#include <QtWebSockets/QWebSocket>
#include <QtWebSockets/QWebSocketServer>
#include <QtWidgets/QApplication>

#include <unordered_set>

#define DS_SEND_WARN(c, m)   \
    QString e(m);            \
    sendErrorToClient(c, e); \
    qWarning() << e;

using namespace std::placeholders;

DataServer::DataServer(QObject* parent) :
    QObject(parent),
    m_pWebSocketServer(nullptr),
    m_Methods{{"subscribe", std::bind(&DataServer::handleMethodSubscribe, this, _1, _2)},
              {"query", std::bind(&DataServer::handleMethodQuery, this, _1, _2)},
              {"auth", std::bind(&DataServer::handleMethodAuth, this, _1, _2)},
              {"mutate", std::bind(&DataServer::handleMethodMutate, this, _1, _2)}},
    m_InternalQueryTargets{{"settings", std::bind(&DataServer::handleQuerySettings, this, _1, _2, _3)},
                           {"launcher", std::bind(&DataServer::handleQueryLauncher, this, _1, _2, _3)}},
    m_MutateTargets{{"settings", std::bind(&DataServer::handleMutateSettings, this, _1, _2)}}
{
    qRegisterMetaType<AppLauncherData>("AppLauncherData");
    qRegisterMetaTypeStreamOperators<AppLauncherData>("AppLauncherData");

    m_pWebSocketServer = new QWebSocketServer(QStringLiteral("Data Server"), QWebSocketServer::SecureMode, this);

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

    m_LauncherMap = settings.value(QUASAR_CONFIG_LAUNCHERMAP).toMap();
}

DataServer::~DataServer()
{
    m_Methods.clear();

    m_Extensions.clear();

    m_AuthedClientsMap.clear();

    m_AuthCodeMap.clear();

    m_pWebSocketServer->close();
}

bool DataServer::findExtension(QString extcode)
{
    std::shared_lock<std::shared_mutex> lk(m_ExtensionsMtx);
    return (m_Extensions.count(extcode) > 0);
}

QString DataServer::generateAuthCode(QString ident, ClientAccessLevel lvl)
{
    quint64 v  = QRandomGenerator::global()->generate64();
    QString hv = QString::number(v, 16).toUpper();

    std::unique_lock<std::mutex> lk(m_AuthCodeMtx);
    m_AuthCodeMap.insert({hv, {ident, lvl, system_clock::now() + 10s}});

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
    std::unique_lock<std::shared_mutex> lk(m_ExtensionsMtx);

    for (QFileInfo& file : list)
    {
        QString libpath = file.path() + "/" + file.fileName();

        qInfo() << "Loading data extension" << libpath;

        DataExtension* extn = DataExtension::load(libpath, this);

        if (!extn)
        {
            qWarning() << "Failed to load extension" << libpath;
        }
        else if (m_InternalQueryTargets.count(extn->getName()))
        {
            qWarning() << "The extension code" << extn->getName() << " is reserved. Unloading " << libpath;
        }
        else if (m_Extensions.count(extn->getName()))
        {
            qWarning() << "Extension with code " << extn->getName() << " already loaded. Unloading" << libpath;
        }
        else
        {
            qInfo() << "Extension " << extn->getName() << " loaded.";
            m_Extensions[extn->getName()].reset(extn);
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

    if (!m_Methods.count(mtd))
    {
        DS_SEND_WARN(sender, "Unknown method type " + mtd);
        return;
    }

    m_Methods[mtd](req, sender);
}

void DataServer::handleMethodSubscribe(const QJsonObject& req, QWebSocket* sender)
{
    QString widgetName;

    {
        std::shared_lock<std::shared_mutex> lk(m_AuthedClientsMtx);

        auto it = m_AuthedClientsMap.find(sender);
        if (it == m_AuthedClientsMap.end())
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
    QString extparm = parms["params"].toString();

    std::shared_lock<std::shared_mutex> lk(m_ExtensionsMtx);

    if (!m_Extensions.count(extcode))
    {
        DS_SEND_WARN(sender, "Unknown extension code " + extcode);
        return;
    }

    QStringList dlist = extparm.split(',', QString::SkipEmptyParts);

    for (QString& src : dlist)
    {
        if (m_Extensions[extcode]->addSubscriber(src, sender, widgetName))
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
    client_data_t clidat;

    {
        std::shared_lock<std::shared_mutex> lk(m_AuthedClientsMtx);

        auto it = m_AuthedClientsMap.find(sender);
        if (it == m_AuthedClientsMap.end())
        {
            DS_SEND_WARN(sender, "Unauthenticated client");
            return;
        }

        clidat = it->second;
    }

    auto parms = req["params"].toObject();

    if (parms.count() != 2)
    {
        DS_SEND_WARN(sender, "Invalid parameters for method 'query'");
        return;
    }

    QString extcode = parms["target"].toString();
    QString extparm = parms["params"].toString();

    if (m_InternalQueryTargets.count(extcode))
    {
        m_InternalQueryTargets[extcode](extparm, clidat, sender);
        return;
    }

    std::shared_lock<std::shared_mutex> lk(m_ExtensionsMtx);

    if (!m_Extensions.count(extcode))
    {
        DS_SEND_WARN(sender, "Unknown extension code " + extcode);
        return;
    }

    m_Extensions[extcode]->pollAndSendData(extparm, sender, clidat.ident);
}

void DataServer::handleMethodAuth(const QJsonObject& req, QWebSocket* sender)
{
    {
        std::shared_lock<std::shared_mutex> lk(m_AuthedClientsMtx);
        if (m_AuthedClientsMap.count(sender))
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

    std::unique_lock<std::mutex> lk(m_AuthCodeMtx);

    auto it = m_AuthCodeMap.find(authcode);
    if (it == m_AuthCodeMap.end())
    {
        DS_SEND_WARN(sender, "Invalid authentication code");
        return;
    }

    std::unique_lock<std::shared_mutex> lkm(m_AuthedClientsMtx);
    m_AuthedClientsMap.insert({sender, it->second});

    qInfo() << "Widget ident " << it->second.ident << " authenticated.";

    m_AuthCodeMap.erase(it);
}

void DataServer::handleMethodMutate(const QJsonObject& req, QWebSocket* sender)
{
    client_data_t clidat;

    {
        std::shared_lock<std::shared_mutex> lk(m_AuthedClientsMtx);

        auto it = m_AuthedClientsMap.find(sender);
        if (it == m_AuthedClientsMap.end())
        {
            DS_SEND_WARN(sender, "Unauthenticated client");
            return;
        }

        clidat = it->second;
    }

    if (clidat.access < CAL_SETTINGS)
    {
        DS_SEND_WARN(sender, "Insufficient access for mutate method");
        return;
    }

    auto parms = req["params"].toObject();

    if (parms.count() != 2)
    {
        DS_SEND_WARN(sender, "Invalid parameters for method 'mutate'");
        return;
    }

    QString targ = parms["target"].toString();

    if (!m_MutateTargets.count(targ))
    {
        DS_SEND_WARN(sender, "Unknown mutate target " + targ);
        return;
    }

    m_MutateTargets[targ](parms["params"], sender);
}

void DataServer::handleQuerySettings(QString params, client_data_t client, QWebSocket* sender)
{
    if (client.access < CAL_SETTINGS)
    {
        DS_SEND_WARN(sender, "Insufficient access for query target 'settings'");
        return;
    }

    // handle params
    auto plist = QSet<QString>::fromList(params.split(',', QString::SkipEmptyParts));

    // compile all settings into json
    QSettings settings;

    QJsonObject sjson;

    // global
    if (plist.contains("all") || plist.contains("global"))
    {
        QJsonObject global;

        global["dataport"] = settings.value(QUASAR_CONFIG_PORT, QUASAR_DATA_SERVER_DEFAULT_PORT).toInt();
        global["loglevel"] = settings.value(QUASAR_CONFIG_LOGLEVEL, QUASAR_CONFIG_DEFAULT_LOGLEVEL).toInt();
        global["savelog"]  = settings.value(QUASAR_CONFIG_LOGFILE, false).toBool();
        global["cookies"]  = QString();

        auto bcookies = settings.value(QUASAR_CONFIG_COOKIES, QByteArray()).toByteArray();
        if (!bcookies.isEmpty())
        {
            global["cookies"] = QString::fromUtf8(qUncompress(bcookies));
        }

#ifdef Q_OS_WIN
        // ------------------Startup launch
        QString   startupFolder = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation) + "/Startup";
        QFileInfo lnk(startupFolder + "/Quasar.lnk");

        global["startup"] = lnk.exists();
#else
        global["startup"] = false;
#endif

        sjson["global"] = global;
    }

    // extensions
    if (plist.contains("all") || plist.contains("extensions"))
    {
        QJsonArray extensions;

        {
            std::shared_lock<std::shared_mutex> lk(m_ExtensionsMtx);

            for (auto& iext : m_Extensions)
            {
                extensions.append(iext.second->getMetadataJSON(false));
            }
        }

        sjson["extensions"] = extensions;
    }

    // launcher
    if (plist.contains("all") || plist.contains("launcher"))
    {
        QJsonArray launcher;

        {
            std::shared_lock<std::shared_mutex> lock(m_LauncherMtx);

            // need to convert to table format...
            for (auto& e : m_LauncherMap)
            {
                if (e.canConvert<AppLauncherData>())
                {
                    auto [command, file, start, args, icon] = e.value<AppLauncherData>();

                    QJsonObject entry;
                    entry["command"] = command;
                    entry["file"]    = file;
                    entry["args"]    = args;
                    entry["start"]   = start;
                    entry["icon"]    = QString();

                    if (!icon.isEmpty())
                    {
                        entry["icon"] = QString::fromUtf8(qUncompress(icon));
                    }

                    launcher.append(entry);
                }
            }
        }

        sjson["launcher"] = launcher;
    }

    auto sdata = QJsonObject{{"data", QJsonObject{{"settings", sjson}}}};

    // send
    QJsonDocument doc(sdata);
    sender->sendTextMessage(QString::fromUtf8(doc.toJson()));
}

void DataServer::handleQueryLauncher(QString params, client_data_t client, QWebSocket* sender)
{
    // If get, send data
    if (params == "get")
    {
        QJsonArray launcher;

        {
            std::shared_lock<std::shared_mutex> lock(m_LauncherMtx);

            // need to convert to table format...
            for (auto& e : m_LauncherMap)
            {
                if (e.canConvert<AppLauncherData>())
                {
                    auto [command, file, start, args, icon] = e.value<AppLauncherData>();

                    QJsonObject entry;
                    entry["command"] = command;
                    entry["file"]    = file;
                    entry["args"]    = args;
                    entry["start"]   = start;
                    entry["icon"]    = QString();

                    if (!icon.isEmpty())
                    {
                        entry["icon"] = QString::fromUtf8(qUncompress(icon));
                    }

                    launcher.append(entry);
                }
            }
        }

        auto sdata = QJsonObject{{"data", QJsonObject{{"launcher", launcher}}}};

        // send
        QJsonDocument doc(sdata);
        sender->sendTextMessage(QString::fromUtf8(doc.toJson()));
        return;
    }

    // Otherwise launch code
    std::shared_lock<std::shared_mutex> lock(m_LauncherMtx);

    if (!m_LauncherMap.contains(params))
    {
        qWarning() << "Launcher command " << params << " not defined";
        return;
    }

    AppLauncherData d;

    QVariant& v = m_LauncherMap[params];

    if (v.canConvert<AppLauncherData>())
    {
        d = v.value<AppLauncherData>();

        QString cmd = d.file;

        if (cmd.contains("://"))
        {
            // treat as url
            qInfo() << "Launching URL " << cmd;
            QDesktopServices::openUrl(QUrl(cmd));
        }
        else
        {
            qInfo() << "Launching " << cmd << d.arguments;

            // treat as file/command
            QFileInfo info(cmd);

            if (info.exists())
            {
                cmd = info.canonicalFilePath();
            }

            bool result = QProcess::startDetached(cmd, QStringList() << d.arguments, d.startpath);

            if (!result)
            {
                qWarning() << "Failed to launch " << cmd;
            }
        }
    }
}

void DataServer::handleMutateSettings(QJsonValue val, QWebSocket* sender)
{
    QJsonObject data = val.toObject();

    if (data.isEmpty())
    {
        qWarning() << "No mutation data";
        return;
    }

    QSettings settings;

    for (auto& key : data.keys())
    {
        if (key == "launcher")
        {
            QVariantMap newmap;

            // deal with launcher table
            auto ltab = data["launcher"].toArray();
            for (auto& e : ltab)
            {
                QJsonObject eobj = e.toObject();
                if (!eobj.isEmpty())
                {
                    AppLauncherData ldat;
                    ldat.command   = eobj["command"].toString();
                    ldat.file      = eobj["file"].toString();
                    ldat.arguments = eobj["args"].toString();
                    ldat.startpath = eobj["start"].toString();
                    ldat.icon      = QByteArray();

                    auto icon = eobj["icon"].toString().toUtf8();
                    if (!icon.isEmpty())
                    {
                        ldat.icon = qCompress(icon);
                    }

                    newmap[ldat.command] = QVariant::fromValue(ldat);
                }
            }

            std::unique_lock<std::shared_mutex> lock(m_LauncherMtx);
            m_LauncherMap = newmap;

            settings.setValue(QUASAR_CONFIG_LAUNCHERMAP, m_LauncherMap);
        }
        else if (key == "extensions")
        {
            auto exts = data["extensions"].toObject();

            std::shared_lock<std::shared_mutex> lk(m_ExtensionsMtx);

            for (auto& extkey : exts.keys())
            {
                if (!m_Extensions.count(extkey))
                {
                    // Extension doesn't exist
                    qWarning() << "Unknown extension code " << extkey;
                    continue;
                }

                auto& ext = m_Extensions[extkey];
                ext->setAllSettings(exts[extkey].toObject());
            }
        }
        else if (key.startsWith("global"))
        {
            // handle global group
            if (key == "global/startup")
            {
                // windows only
#ifdef Q_OS_WIN

                QString   startupFolder = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation) + "/Startup";
                QString   filename      = startupFolder + "/Quasar.lnk";
                QFileInfo lnk(filename);

                bool checked = data[key].toBool();

                if (checked && !lnk.exists())
                {
                    QFile::link(QCoreApplication::applicationFilePath(), filename);
                }
                else if (!checked && lnk.exists())
                {
                    QFile::remove(filename);
                }
#endif
            }
            else if (key == "global/cookies")
            {
                // compress cookies file
                auto rcook   = data[key].toString().toUtf8();
                auto cookies = QByteArray();

                if (!rcook.isEmpty())
                {
                    cookies = qCompress(rcook);
                }

                settings.setValue(key, cookies);
            }
            else
            {
                settings.setValue(key, data[key].toVariant());
            }
        }
        else
        {
            qWarning() << "Unknown setting key " << key;
            continue;
        }
    }

    qInfo() << "All settings saved!";
}

void DataServer::checkAuth(QWebSocket* client)
{
    bool disconnect = false;

    {
        // Check auth
        std::shared_lock<std::shared_mutex> lk(m_AuthedClientsMtx);
        if (!m_AuthedClientsMap.count(client))
        {
            // unauthenticated client, cut the connection
            disconnect = true;
        }
    }

    {
        // Clean expired auth codes
        std::unique_lock<std::mutex> lk(m_AuthCodeMtx);
        for (auto it = m_AuthCodeMap.begin(); it != m_AuthCodeMap.end();)
        {
            if (it->second.expiry < system_clock::now())
            {
                it = m_AuthCodeMap.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    if (disconnect)
    {
        DS_SEND_WARN(client, "Unauthenticated client, disconnecting");
        client->close(QWebSocketProtocol::CloseCodeNormal, "Unauthenticated client");
    }
}

void DataServer::sendErrorToClient(QWebSocket* client, QString err)
{
    // Craft error json msg
    QJsonObject msg;
    msg["errors"] = err;

    QJsonDocument doc(msg);

    client->sendTextMessage(QString::fromUtf8(doc.toJson()));
}

void DataServer::onNewConnection()
{
    QWebSocket* pSocket = m_pWebSocketServer->nextPendingConnection();

    pSocket->setParent(this);
    connect(pSocket, &QWebSocket::textMessageReceived, this, &DataServer::processMessage);
    connect(pSocket, &QWebSocket::disconnected, this, &DataServer::socketDisconnected);

    QTimer* authtimer = new QTimer(pSocket);
    authtimer->setSingleShot(true);

    connect(authtimer, &QTimer::timeout, this, [this, pSocket, authtimer] {
        checkAuth(pSocket);
        authtimer->deleteLater();
    });

    authtimer->start(10000);
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
            std::shared_lock<std::shared_mutex> lk(m_ExtensionsMtx);
            for (auto& p : m_Extensions)
            {
                p.second->removeSubscriber(pClient);
            }
        }

        {
            std::unique_lock<std::shared_mutex> lkm(m_AuthedClientsMtx);
            m_AuthedClientsMap.erase(pClient);
        }

        pClient->deleteLater();
    }
}
