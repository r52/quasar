#pragma once

#include <plugin_types.h>

#include <chrono>
#include <QtCore/QObject>
#include <QSet>
#include <QMap>

QT_FORWARD_DECLARE_CLASS(QWebSocket)
QT_FORWARD_DECLARE_CLASS(QTimer)

#define QUASAR_DP_ENABLED_PREFIX "enabled_"
#define QUASAR_DP_REFRESH_PREFIX "refresh_"

struct DataSource
{
    QString key;
    size_t uid;
    uint32_t refreshmsec;
    QTimer *timer = nullptr;
    QSet<QWebSocket *> subscribers;
};

using DataSourceMapType = QMap<QString, DataSource>;

class DataPlugin : public QObject
{
    Q_OBJECT;

public:
    ~DataPlugin();

    static unsigned int _uid;

    static DataPlugin* load(QString libpath, QObject *parent = Q_NULLPTR);

    using plugin_load = std::add_pointer_t<quasar_plugin_info_t*(void)>;

    bool setupPlugin();

    bool addSubscriber(QString source, QWebSocket *subscriber, QString widgetName);
    void removeSubscriber(QWebSocket *subscriber);

    QString getLibPath() { return m_libpath; };
    QString getName() { return m_name; };
    QString getCode() { return m_code; };
    QString getDesc() { return m_desc; };
    QString getAuthor() { return m_author; };
    QString getVersion() { return m_version; };
    QString getSettingsCode(QString key) { return "plugin_" + getCode() + "/" + key; };

    DataSourceMapType& getDataSources() { return m_datasources; };

    void setDataSourceEnabled(QString source, bool enabled);

    void setDataSourceRefresh(QString source, uint32_t msec);

private slots:
    void getAndSendData(DataSource& source);

private:
    DataPlugin(quasar_plugin_info_t* p, QString path, QObject *parent = Q_NULLPTR);

    void createTimer(DataSource& data);

    quasar_plugin_info_t* plugin;

    bool m_Initialized = false;

    QString m_libpath;
    QString m_name;
    QString m_code;
    QString m_desc;
    QString m_author;
    QString m_version;

    DataSourceMapType m_datasources;
};
