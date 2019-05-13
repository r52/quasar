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

    QSettings settings;

    bool secureSock = settings.value(QUASAR_CONFIG_SECURE, QUASAR_DATA_SERVER_DEFAULT_SECURE).toBool();

    m_pWebSocketServer = new QWebSocketServer(QStringLiteral("Data Server"), secureSock ? QWebSocketServer::SecureMode : QWebSocketServer::NonSecureMode, this);

    if (secureSock)
    {
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
    }

    quint16 port = settings.value(QUASAR_CONFIG_PORT, QUASAR_DATA_SERVER_DEFAULT_PORT).toUInt();

    if (!m_pWebSocketServer->listen(QHostAddress::LocalHost, port))
    {
        qWarning() << "Data server failed to bind port" << port;
    }
    else
    {
        qInfo().noquote().nospace() << "Data Server " << (secureSock ? "(wss)" : "(ws)") << " running locally on port " << port;

        connect(m_pWebSocketServer, &QWebSocketServer::newConnection, this, &DataServer::onNewConnection);

        loadExtensions();
    }

    m_LauncherMap = settings.value(QUASAR_CONFIG_LAUNCHERMAP).toMap();
    m_UserKeysMap = settings.value(QUASAR_CONFIG_USERKEYSMAP).toMap();
}

DataServer::~DataServer()
{
    m_Methods.clear();

    m_Extensions.clear();

    m_AuthedClientsSet.clear();

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
    quint64 h  = QRandomGenerator::global()->generate64();
    quint64 l  = QRandomGenerator::global()->generate64();
    QString hv = QString::number(h, 16).toUpper() + QString::number(l, 16).toUpper();

    client_data_t cdt = {ident, lvl, system_clock::now() + 10s};

    std::unique_lock<std::mutex> lk(m_AuthCodeMtx);
    m_AuthCodeMap.insert({hv, cdt});

    return hv;
}

void DataServer::loadExtensions()
{
    QDir          dir("extensions/");
    QFileInfoList list = dir.entryInfoList(QStringList() << "*.dll"
                                                         << "*.so"
                                                         << "*.dylib",
                                           QDir::Files);

#ifdef Q_OS_WIN
    // check windows doc path
    auto extdir = QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/Quasar/extensions");
    list.append(extdir.entryInfoList(QStringList() << "*.dll", QDir::Files));
#endif

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

        DataExtension* extn = DataExtension::load(libpath);

        if (!extn)
        {
            qWarning() << "Failed to load extension" << libpath;
        }
        else if (m_InternalQueryTargets.count(extn->getName()))
        {
            qWarning() << "The extension code" << extn->getName() << "is reserved. Unloading" << libpath;
        }
        else if (m_Extensions.count(extn->getName()))
        {
            qWarning() << "Extension with code" << extn->getName() << "already loaded. Unloading" << libpath;
        }
        else
        {
            qInfo() << "Extension" << extn->getName() << "loaded.";
            m_Extensions[extn->getName()].reset(new ExtensionControl(extn));
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
    {
        std::shared_lock<std::shared_mutex> lk(m_AuthedClientsMtx);

        auto it = m_AuthedClientsSet.find(sender);
        if (it == m_AuthedClientsSet.end())
        {
            DS_SEND_WARN(sender, "Unauthenticated client");
            return;
        }
    }

    auto qvar = sender->property(WGT_PROP_IDENTITY);
    if (!qvar.isValid())
    {
        qCritical() << "Invalid identity in authenticated WebSocket connection.";
        return;
    }

    client_data_t clidat     = qvar.value<client_data_t>();
    QString       widgetName = clidat.ident;

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
        DS_SEND_WARN(sender, "Unknown extension identifier " + extcode);
        return;
    }

    QStringList dlist = extparm.split(',', QString::SkipEmptyParts);

    for (QString& src : dlist)
    {
        bool result = false;
        auto extn   = m_Extensions[extcode]->getExtension();

        QMetaObject::invokeMethod(
            extn, [=] { return extn->addSubscriber(src, sender, widgetName); }, Qt::BlockingQueuedConnection, &result);

        if (result)
        {
            qInfo() << "Widget" << widgetName << "subscribed to extension" << extcode << "data" << src;
        }
        else
        {
            DS_SEND_WARN(sender, "Failed to subscribed to extension " + extcode + " data " + src);
        }
    }
}

void DataServer::handleMethodQuery(const QJsonObject& req, QWebSocket* sender)
{
    {
        std::shared_lock<std::shared_mutex> lk(m_AuthedClientsMtx);

        auto it = m_AuthedClientsSet.find(sender);
        if (it == m_AuthedClientsSet.end())
        {
            DS_SEND_WARN(sender, "Unauthenticated client");
            return;
        }
    }

    auto qvar = sender->property(WGT_PROP_IDENTITY);
    if (!qvar.isValid())
    {
        qCritical() << "Invalid identity in authenticated WebSocket connection.";
        return;
    }

    client_data_t clidat = qvar.value<client_data_t>();

    auto parms = req["params"].toObject();

    if (parms.count() != 2)
    {
        DS_SEND_WARN(sender, "Invalid parameters for method 'query'");
        return;
    }

    QString extcode = parms["target"].toString();
    QString extparm = parms["params"].toString();
    QString extargs = parms["args"].toString();

    if (m_InternalQueryTargets.count(extcode))
    {
        m_InternalQueryTargets[extcode](extparm, clidat, sender);
        return;
    }

    std::shared_lock<std::shared_mutex> lk(m_ExtensionsMtx);

    if (!m_Extensions.count(extcode))
    {
        DS_SEND_WARN(sender, "Unknown extension identifier " + extcode);
        return;
    }

    auto extn = m_Extensions[extcode]->getExtension();

    QMetaObject::invokeMethod(extn, [=] { extn->pollAndSendData(extparm, extargs, sender, clidat.ident); });
}

void DataServer::handleMethodAuth(const QJsonObject& req, QWebSocket* sender)
{
    {
        std::shared_lock<std::shared_mutex> lk(m_AuthedClientsMtx);
        if (m_AuthedClientsSet.count(sender))
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

    client_data_t clident;

    if (!authenticateClient(sender, authcode, clident))
    {
        DS_SEND_WARN(sender, "Invalid authentication code");
        return;
    }

    sender->setProperty(WGT_PROP_IDENTITY, QVariant::fromValue(clident));

    std::unique_lock<std::shared_mutex> lkm(m_AuthedClientsMtx);
    m_AuthedClientsSet.insert(sender);

    qInfo() << "Widget identity" << clident.ident << "authenticated.";
}

void DataServer::handleMethodMutate(const QJsonObject& req, QWebSocket* sender)
{
    {
        std::shared_lock<std::shared_mutex> lk(m_AuthedClientsMtx);

        auto it = m_AuthedClientsSet.find(sender);
        if (it == m_AuthedClientsSet.end())
        {
            DS_SEND_WARN(sender, "Unauthenticated client");
            return;
        }
    }

    auto qvar = sender->property(WGT_PROP_IDENTITY);
    if (!qvar.isValid())
    {
        qCritical() << "Invalid identity in authenticated WebSocket connection.";
        return;
    }

    client_data_t clidat = qvar.value<client_data_t>();

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

        global["secure"]   = settings.value(QUASAR_CONFIG_SECURE, QUASAR_DATA_SERVER_DEFAULT_SECURE).toBool();
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
                QJsonObject result;
                auto        extn = iext.second->getExtension();

                QMetaObject::invokeMethod(
                    extn, [=] { return extn->getMetadataJSON(false); }, Qt::BlockingQueuedConnection, &result);

                extensions.append(result);
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

    // launcher
    if (plist.contains("all") || plist.contains("userkeys"))
    {
        QJsonArray userkeys;

        {
            std::shared_lock<std::shared_mutex> lock(m_UserKeysMtx);

            // need to convert to table format...
            for (auto it = m_UserKeysMap.begin(); it != m_UserKeysMap.end(); ++it)
            {
                auto name = it.value().toString();

                QJsonObject entry;
                entry["name"] = name;
                entry["key"]  = it.key();

                userkeys.append(entry);
            }
        }

        sjson["userkeys"] = userkeys;
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

                    // For regular clients, only include command and icon data
                    QJsonObject entry;
                    entry["command"] = command;
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
        qWarning() << "Launcher command" << params << "not defined";
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
            qInfo() << "Launching URL" << cmd;
            QDesktopServices::openUrl(QUrl(cmd));
        }
        else
        {
            qInfo() << "Launching" << cmd << d.arguments;

            // treat as file/command
            QFileInfo info(cmd);

            if (info.exists())
            {
                cmd = info.canonicalFilePath();
            }

            bool result = QProcess::startDetached(cmd, QStringList() << d.arguments, d.startpath);

            if (!result)
            {
                qWarning() << "Failed to launch" << cmd;
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
                    qWarning() << "Unknown extension name" << extkey;
                    continue;
                }

                auto extn   = m_Extensions[extkey]->getExtension();
                auto setobj = exts[extkey].toObject();
                QMetaObject::invokeMethod(extn, [=] { extn->setAllSettings(setobj); });
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
        else if (key == "userkeys")
        {
            QVariantMap newmap;

            // deal with launcher table
            auto ltab = data["userkeys"].toArray();
            for (auto& e : ltab)
            {
                QJsonObject eobj = e.toObject();
                if (!eobj.isEmpty())
                {
                    auto name   = eobj["name"].toString();
                    auto key    = eobj["key"].toString();
                    newmap[key] = name;
                }
            }

            std::unique_lock<std::shared_mutex> lock(m_UserKeysMtx);
            m_UserKeysMap = newmap;

            settings.setValue(QUASAR_CONFIG_USERKEYSMAP, m_UserKeysMap);
        }
        else
        {
            qWarning() << "Unknown setting key" << key;
            continue;
        }
    }

    qInfo() << "All settings saved!";
}

bool DataServer::authenticateClient(QWebSocket* client, QString code, client_data_t& clidat)
{
    // Check user keys
    {
        std::unique_lock<std::shared_mutex> lk(m_UserKeysMtx);

        auto it = m_UserKeysMap.find(code);
        if (it != m_UserKeysMap.end())
        {
            auto keyname = it.value().toString();

            // User key exists, check if it is in use
            if (m_UserKeysInUse.count(code))
            {
                // Key is in use
                DS_SEND_WARN(client, "User key " + keyname + " already in use.");
                return false;
            }

            client_data_t cdt;
            cdt.ident  = keyname;
            cdt.access = CAL_WIDGET;

            // Otherwise, log them in
            m_UserKeysInUse.insert(code);
            clidat = cdt;
            client->setProperty(WGT_PROP_USERKEY, code);
            return true;
        }
        // Otherwise, pass thru to regular authcode check
    }

    // Check auth codes
    std::unique_lock<std::mutex> lk(m_AuthCodeMtx);

    auto it = m_AuthCodeMap.find(code);
    if (it == m_AuthCodeMap.end())
    {
        return false;
    }

    clidat = it->second;

    m_AuthCodeMap.erase(it);

    return true;
}

void DataServer::checkAuth(QWebSocket* client)
{
    bool disconnect = false;

    {
        // Check auth
        std::shared_lock<std::shared_mutex> lk(m_AuthedClientsMtx);
        if (!m_AuthedClientsSet.count(client))
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
                auto extn = p.second->getExtension();

                QMetaObject::invokeMethod(extn, [=] { extn->removeSubscriber(pClient); });
            }
        }

        {
            std::unique_lock<std::shared_mutex> lkm(m_AuthedClientsMtx);
            m_AuthedClientsSet.erase(pClient);
        }

        auto keyprop = pClient->property(WGT_PROP_USERKEY);
        if (keyprop.isValid())
        {
            // If this client was connected using a user key, free it
            std::unique_lock<std::shared_mutex> lk(m_UserKeysMtx);

            m_UserKeysInUse.erase(keyprop.toString());
        }

        pClient->deleteLater();
    }
}

ExtensionControl::ExtensionControl(DataExtension* ext) : m_ext(ext)
{
    if (!m_ext)
    {
        throw std::invalid_argument("null extension");
    }

    m_ext->moveToThread(&extThread);
    connect(&extThread, &QThread::finished, m_ext, &QObject::deleteLater);
    extThread.setObjectName(ext->getName() + " thread");
    extThread.start();

    // Initialize and wait until finished
    QMetaObject::invokeMethod(
        m_ext, [=] { m_ext->initialize(); }, Qt::BlockingQueuedConnection);
}

ExtensionControl::~ExtensionControl()
{
    extThread.quit();
    extThread.wait();
}
