#pragma once

#include "sharedlocker.h"
#include <qstring_hash_impl.h>

#include <QObject>
#include <functional>
#include <memory>
#include <unordered_map>

QT_FORWARD_DECLARE_CLASS(QWebSocketServer)
QT_FORWARD_DECLARE_CLASS(QWebSocket)

class DataPlugin;

using DataPluginMapType = std::unordered_map<QString, std::unique_ptr<DataPlugin>>;
using HandlerFuncType   = std::function<void(const QJsonObject&, QWebSocket*)>;

class DataServer : public QObject
{
    friend class DataServices;

    Q_OBJECT

    using HandleReqCallMapType = std::unordered_map<QString, HandlerFuncType>;

public:
    ~DataServer();

    auto getPlugins() { return make_shared_locker<DataPluginMapType>(&m_plugins, &m_mutex); }

    bool addHandler(QString type, HandlerFuncType handler);
    bool findPlugin(QString pluginCode);

private:
    void loadDataPlugins();
    void handleRequest(const QJsonObject& req, QWebSocket* sender);

    void handleSubscribeReq(const QJsonObject& req, QWebSocket* sender);
    void handlePollReq(const QJsonObject& req, QWebSocket* sender);

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

    HandleReqCallMapType      m_reqcallmap;
    QWebSocketServer*         m_pWebSocketServer;
    DataPluginMapType         m_plugins;
    mutable std::shared_mutex m_mutex;
};
