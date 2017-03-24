#pragma once

#include "plugin_types.h"

#include <chrono>
#include <QtCore/QObject>
#include <QSet>
#include <QMap>

QT_FORWARD_DECLARE_CLASS(QWebSocket)
QT_FORWARD_DECLARE_CLASS(QTimer)

struct DataSource
{
    unsigned int refreshmsec;
    QTimer *timer = nullptr;
    QSet<QWebSocket *> subscribers;
};

class DataPlugin : public QObject
{
    Q_OBJECT;

public:
    ~DataPlugin();

    static DataPlugin* load(QString libpath, QObject *parent = Q_NULLPTR);

    typedef void(*plugin_free)(void*, int);
    typedef int(*plugin_init)(QuasarPluginInfo*);
    typedef int(*plugin_get_data)(const char*, char*, int, int*);

    const plugin_free free;
    const plugin_init init;
    const plugin_get_data getData;

    bool setupPlugin();

    bool addSubscriber(QString source, QWebSocket *subscriber, QString widgetName);
    void removeSubscriber(QWebSocket *subscriber);

    QString getLibPath() { return m_libpath; };
    QString getName() { return m_name; };
    QString getCode() { return m_code; };
    QString getDesc() { return m_desc; };
    QString getAuthor() { return m_author; };

private slots:
    void getAndSendData(QString source);

private:
    DataPlugin(plugin_free freeFunc, plugin_init initFunc, plugin_get_data getDataFunc, QString path, QObject *parent = Q_NULLPTR);

    bool m_Initialized = false;

    QString m_libpath;
    QString m_name;
    QString m_code;
    QString m_desc;
    QString m_author;

    QMap<QString, DataSource> m_datasources;
};
