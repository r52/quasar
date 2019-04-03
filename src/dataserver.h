#pragma once

#include <qstring_hash_impl.h>

#include <QObject>
#include <QVariant>

#include <chrono>
#include <functional>
#include <memory>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>

QT_FORWARD_DECLARE_CLASS(QWebSocketServer)
QT_FORWARD_DECLARE_CLASS(QWebSocket)

class DataExtension;

using namespace std::chrono;

struct AppLauncherData
{
    QString    command;
    QString    file;
    QString    startpath;
    QString    arguments;
    QByteArray icon; // icon data is compressed by qCompress()

    friend QDataStream& operator<<(QDataStream& s, const AppLauncherData& o)
    {
        s << o.command;
        s << o.file;
        s << o.startpath;
        s << o.arguments;
        s << o.icon;
        return s;
    }

    friend QDataStream& operator>>(QDataStream& s, AppLauncherData& o)
    {
        s >> o.command;
        s >> o.file;
        s >> o.startpath;
        s >> o.arguments;
        s >> o.icon;
        return s;
    }
};

Q_DECLARE_METATYPE(AppLauncherData);

enum ClientAccessLevel : uint8_t
{
    CAL_WIDGET = 0,
    CAL_SETTINGS,
    CAL_DEBUG
};

struct client_data_t
{
    QString                  ident;
    ClientAccessLevel        access;
    system_clock::time_point expiry;
};

Q_DECLARE_METATYPE(client_data_t);

class DataServer : public QObject
{
    friend class DataServices;

    Q_OBJECT

    using DataExtensionMapType   = std::unordered_map<QString, std::unique_ptr<DataExtension>>;
    using MethodFuncType         = std::function<void(const QJsonObject&, QWebSocket*)>;
    using MethodCallMapType      = std::unordered_map<QString, MethodFuncType>;
    using AuthedClientsSetType   = std::unordered_set<QWebSocket*>;
    using AuthCodesMapType       = std::unordered_map<QString, client_data_t>;
    using InternalTargetFuncType = std::function<void(QString, client_data_t, QWebSocket*)>;
    using InternalTargetMapType  = std::unordered_map<QString, InternalTargetFuncType>;
    using MutateTargetFuncType   = std::function<void(QJsonValue, QWebSocket*)>;
    using MutateTargetMapType    = std::unordered_map<QString, MutateTargetFuncType>;

public:
    ~DataServer();

    bool findExtension(QString extcode);

    QString generateAuthCode(QString ident, ClientAccessLevel lvl = CAL_WIDGET);

private:
    void loadExtensions();
    void handleRequest(const QJsonObject& req, QWebSocket* sender);

    // Method handling
    void handleMethodSubscribe(const QJsonObject& req, QWebSocket* sender);
    void handleMethodQuery(const QJsonObject& req, QWebSocket* sender);
    void handleMethodAuth(const QJsonObject& req, QWebSocket* sender);
    void handleMethodMutate(const QJsonObject& req, QWebSocket* sender);

    // Internal data targets
    void handleQuerySettings(QString params, client_data_t client, QWebSocket* sender);
    void handleQueryLauncher(QString params, client_data_t client, QWebSocket* sender);

    // Mutate targets
    void handleMutateSettings(QJsonValue val, QWebSocket* sender);

    // Helpers
    void checkAuth(QWebSocket* client);
    void sendErrorToClient(QWebSocket* client, QString err);

private slots:
    void onNewConnection();
    void processMessage(QString message);
    void socketDisconnected();

private:
    explicit DataServer(QObject* parent = nullptr);
    DataServer(const DataServer&) = delete;
    DataServer(DataServer&&)      = delete;
    DataServer& operator=(const DataServer&) = delete;
    DataServer& operator=(DataServer&&) = delete;

    QWebSocketServer* m_pWebSocketServer;

    // Method function map
    MethodCallMapType m_Methods;

    // Internal query targets
    InternalTargetMapType m_InternalQueryTargets;

    // Mutate targets
    MutateTargetMapType m_MutateTargets;

    // App launcher
    QVariantMap               m_LauncherMap;
    mutable std::shared_mutex m_LauncherMtx;

    // Authentication code management
    AuthCodesMapType   m_AuthCodeMap;
    mutable std::mutex m_AuthCodeMtx;

    // Authenticated clients management
    AuthedClientsSetType      m_AuthedClientsSet;
    mutable std::shared_mutex m_AuthedClientsMtx;

    // Extensions management
    DataExtensionMapType      m_Extensions;
    mutable std::shared_mutex m_ExtensionsMtx;
};
