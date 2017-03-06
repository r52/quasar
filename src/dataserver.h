#pragma once

#include <QtCore/QObject>
#include <QtCore/QList>
#include <QtCore/QByteArray>
#include <QMap>

QT_FORWARD_DECLARE_CLASS(QWebSocketServer)
QT_FORWARD_DECLARE_CLASS(QWebSocket)
QT_FORWARD_DECLARE_CLASS(Quasar)

class DataPlugin;

class DataServer : public QObject
{
    Q_OBJECT;

public:
    explicit DataServer(Quasar *parent);
    ~DataServer();

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
    QMap<QString, DataPlugin*> m_plugins;
};
