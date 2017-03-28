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
    QString key;
    unsigned int uid;
    unsigned int refreshmsec;
    QTimer *timer = nullptr;
    QSet<QWebSocket *> subscribers;
};

typedef QMap<QString, DataSource> DataSourceMapType;

class DataPlugin : public QObject
{
    Q_OBJECT;

public:
    ~DataPlugin();

    static unsigned int _uid;

    static DataPlugin* load(QString libpath, QObject *parent = Q_NULLPTR);

    typedef void(*plugin_free)(void*, int);
    typedef int(*plugin_init)(int, QuasarPluginInfo*);
    typedef int(*plugin_get_data)(unsigned int, char*, int, int*);

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
    QString getVersion() { return m_version; };

    DataSourceMapType& getDataSources() { return m_datasources; };

private slots:
    void getAndSendData(DataSource& source);

private:
    DataPlugin(plugin_free freeFunc, plugin_init initFunc, plugin_get_data getDataFunc, QString path, QObject *parent = Q_NULLPTR);

    bool m_Initialized = false;

    QString m_libpath;
    QString m_name;
    QString m_code;
    QString m_desc;
    QString m_author;
    QString m_version;

    DataSourceMapType m_datasources;
};
