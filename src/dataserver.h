#pragma once

#include "sharedlocker.h"
#include <qstring_hash_impl.h>

#include <QObject>
#include <chrono>
#include <functional>
#include <memory>
#include <unordered_map>
#include <unordered_set>

QT_FORWARD_DECLARE_CLASS(QWebSocketServer)
QT_FORWARD_DECLARE_CLASS(QWebSocket)

class DataExtension;

using namespace std::chrono;

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

using DataExtensionMapType = std::unordered_map<QString, std::unique_ptr<DataExtension>>;
using HandlerFuncType      = std::function<void(const QJsonObject&, QWebSocket*)>;
using AuthedClientsMapType = std::unordered_map<QWebSocket*, client_data_t>;
using AuthCodesMapType     = std::unordered_map<QString, client_data_t>;

class DataServer : public QObject
{
    friend class DataServices;

    Q_OBJECT

    using HandleReqCallMapType = std::unordered_map<QString, HandlerFuncType>;

public:
    ~DataServer();

    auto getPlugins() { return make_shared_locker<DataExtensionMapType>(&m_extensions, &m_extmutex); }

    bool addMethodHandler(QString type, HandlerFuncType handler);
    bool findExtension(QString extcode);

    QString generateAuthCode(QString ident);

private:
    void loadExtensions();
    void handleRequest(const QJsonObject& req, QWebSocket* sender);

    void handleMethodSubscribe(const QJsonObject& req, QWebSocket* sender);
    void handleMethodQuery(const QJsonObject& req, QWebSocket* sender);
    void handleMethodAuth(const QJsonObject& req, QWebSocket* sender);

    void checkAuth(QWebSocket* client);
    void sendErrorToClient(QWebSocket* client, QString err);

private slots:
    void onNewConnection();
    void processMessage(QString message);
    void socketDisconnected();

private:
    explicit DataServer(QObject* parent = Q_NULLPTR);
    DataServer(const DataServer&) = delete;
    DataServer(DataServer&&)      = delete;
    DataServer& operator=(const DataServer&) = delete;
    DataServer& operator=(DataServer&&) = delete;

    HandleReqCallMapType      m_methodmap;
    QWebSocketServer*         m_pWebSocketServer;
    AuthCodesMapType          m_authcodes;
    mutable std::mutex        m_codemutex;
    AuthedClientsMapType      m_authedclients;
    mutable std::shared_mutex m_authmutex;
    DataExtensionMapType      m_extensions;
    mutable std::shared_mutex m_extmutex;
};
