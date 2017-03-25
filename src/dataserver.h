#pragma once

#include <QtCore/QObject>
#include <QtCore/QList>
#include <QtCore/QByteArray>
#include <QMap>

#define QUASAR_DATA_SERVER_DEFAULT_PORT 13337

QT_FORWARD_DECLARE_CLASS(QWebSocketServer)
QT_FORWARD_DECLARE_CLASS(QWebSocket)
QT_FORWARD_DECLARE_CLASS(Quasar)

class DataPlugin;

typedef QMap<QString, DataPlugin*> DataPluginMapType;

class DataServer : public QObject
{
    Q_OBJECT;

public:
    explicit DataServer(QObject *parent);
    ~DataServer();

    DataPluginMapType& getPlugins() { return m_plugins; };

private:
    void loadDataPlugins();
    void handleRequest(const QJsonObject &req, QWebSocket *sender);

private slots:
    void onNewConnection();
    void processMessage(QString message);
    void socketDisconnected();

private:
    Quasar *m_parent;
    QWebSocketServer *m_pWebSocketServer;
    QList<QWebSocket *> m_clients;
    DataPluginMapType m_plugins;
};
