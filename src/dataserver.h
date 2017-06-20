#pragma once

#include <QList>
#include <QMap>
#include <QObject>
#include <functional>

QT_FORWARD_DECLARE_CLASS(QWebSocketServer)
QT_FORWARD_DECLARE_CLASS(QWebSocket)
QT_FORWARD_DECLARE_CLASS(Quasar)

class DataPlugin;

using DataPluginMapType = QMap<QString, DataPlugin*>;
using HandlerFuncType   = std::function<void(const QJsonObject&, QWebSocket*)>;

class DataServer : public QObject
{
    Q_OBJECT;

    using HandleReqCallMap = QMap<QString, HandlerFuncType>;

public:
    explicit DataServer(QObject* parent);
    ~DataServer();

    DataPluginMapType& getPlugins() { return m_plugins; };

    bool addHandler(QString type, HandlerFuncType handler);

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
    HandleReqCallMap  m_reqcallmap;
    Quasar*           m_parent;
    QWebSocketServer* m_pWebSocketServer;
    DataPluginMapType m_plugins;
};
