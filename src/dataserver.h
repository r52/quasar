#pragma once

#include <QtCore/QObject>
#include <QtCore/QList>
#include <QtCore/QByteArray>
#include <QMap>

QT_FORWARD_DECLARE_CLASS(QWebSocketServer)
QT_FORWARD_DECLARE_CLASS(QWebSocket)

class DataPlugin;

class DataServer : public QObject
{
    Q_OBJECT;

public:
    explicit DataServer(QObject *parent = Q_NULLPTR);
    ~DataServer();

private:
    void loadDataPlugins();
    void handleRequest(const QJsonObject &req, QWebSocket *sender);

private slots:
    void onNewConnection();
    void processMessage(QString message);
    void socketDisconnected();

private:
    QWebSocketServer *m_pWebSocketServer;
    QList<QWebSocket *> m_clients;
    QMap<QString, DataPlugin*> m_plugins;
};
