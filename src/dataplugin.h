#pragma once

#include <plugin_types.h>

#include <QMap>
#include <QObject>
#include <QSet>
#include <memory>

QT_FORWARD_DECLARE_CLASS(QWebSocket)
QT_FORWARD_DECLARE_CLASS(QTimer)

#define QUASAR_DP_ENABLED_PREFIX "enabled_"
#define QUASAR_DP_REFRESH_PREFIX "refresh_"
#define QUASAR_DP_CUSTOM_PREFIX "custom_"

struct DataSource
{
    bool              enabled;
    QString           key;
    size_t            uid;
    int64_t           refreshmsec;
    QTimer*           timer = nullptr;
    QSet<QWebSocket*> subscribers;
};

using DataSourceMapType = QMap<QString, DataSource>;

class DataPlugin : public QObject
{
    Q_OBJECT;

public:
    using plugin_load = std::add_pointer_t<quasar_plugin_info_t*(void)>;

    ~DataPlugin();

    static uintmax_t _uid;

    static DataPlugin* load(QString libpath, QObject* parent = Q_NULLPTR);

    bool setupPlugin();

    bool addSubscriber(QString source, QWebSocket* subscriber, QString widgetName);
    void removeSubscriber(QWebSocket* subscriber);

    void pollAndSendData(QString source, QWebSocket* subscriber, QString widgetName);

    QString getLibPath() { return m_libpath; };
    QString getName() { return m_name; };
    QString getCode() { return m_code; };
    QString getDesc() { return m_desc; };
    QString getAuthor() { return m_author; };
    QString getVersion() { return m_version; };
    QString getSettingsCode(QString key) { return "plugin_" + getCode() + "/" + key; };

    quasar_settings_t* getSettings() { return m_settings.get(); };

    DataSourceMapType& getDataSources() { return m_datasources; };

    void setDataSourceEnabled(QString source, bool enabled);

    void setDataSourceRefresh(QString source, int64_t msec);

    void setCustomSetting(QString name, int val);
    void setCustomSetting(QString name, double val);
    void setCustomSetting(QString name, bool val);

    void updatePluginSettings();

private slots:
    void sendDataToSubscribers(DataSource& source);

private:
    DataPlugin(quasar_plugin_info_t* p, QString path, QObject* parent = Q_NULLPTR);

    void    createTimer(DataSource& data);
    QString craftDataMessage(DataSource& data);

    quasar_plugin_info_t* m_plugin;

    std::unique_ptr<quasar_settings_t> m_settings;

    bool m_Initialized = false;

    QString m_libpath;
    QString m_name;
    QString m_code;
    QString m_desc;
    QString m_author;
    QString m_version;

    DataSourceMapType m_datasources;
};
