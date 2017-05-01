#include "dataplugin.h"

#include <plugin_support_internal.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QLibrary>
#include <QSettings>
#include <QTimer>
#include <QtWebSockets/QWebSocket>

uintmax_t DataPlugin::_uid = 0;

DataPlugin::~DataPlugin()
{
    if (nullptr != m_plugin->shutdown)
    {
        m_plugin->shutdown(this);
    }

    // Do some explicit cleanup
    for (DataSource& src : m_datasources)
    {
        if (nullptr != src.timer)
        {
            delete src.timer;
            src.timer = nullptr;
        }

        if (nullptr != src.locks)
        {
            delete src.locks;
            src.locks = nullptr;
        }

        src.subscribers.clear();
    }

    // plugin is responsible for cleanup of quasar_plugin_info_t*
    m_plugin = nullptr;
}

DataPlugin* DataPlugin::load(QString libpath, QObject* parent /*= Q_NULLPTR*/)
{
    QLibrary lib(libpath);

    if (!lib.load())
    {
        qWarning() << lib.errorString();
        return nullptr;
    }

    plugin_load loadfunc = (plugin_load) lib.resolve("quasar_plugin_load");

    if (loadfunc)
    {
        quasar_plugin_info_t* p = loadfunc();

        if (p && p->init && p->shutdown && p->get_data)
        {
            DataPlugin* plugin = new DataPlugin(p, libpath, parent);
            return plugin;
        }
        else
        {
            qWarning() << "quasar_plugin_load failed in" << libpath;
        }
    }
    else
    {
        qWarning() << "Failed to resolve plugin API in" << libpath;
    }

    return nullptr;
}

bool DataPlugin::setupPlugin()
{
    if (m_Initialized)
    {
        qDebug() << "Plugin '" << m_code << "' already initialized";
        return true;
    }

    m_name    = m_plugin->name;
    m_code    = m_plugin->code;
    m_author  = m_plugin->author;
    m_desc    = m_plugin->description;
    m_version = m_plugin->version;

    QSettings settings;

    if (nullptr != m_plugin->dataSources)
    {
        for (unsigned int i = 0; i < m_plugin->numDataSources; i++)
        {
            if (m_datasources.contains(m_plugin->dataSources[i].dataSrc))
            {
                qWarning() << "Plugin " << m_code << " tried to register more than one data source '" << m_plugin->dataSources[i].dataSrc << "'";
                continue;
            }

            qInfo() << "Plugin " << m_code << " registering data source '" << m_plugin->dataSources[i].dataSrc << "'";

            DataSource& source = m_datasources[m_plugin->dataSources[i].dataSrc];
            source.key         = m_plugin->dataSources[i].dataSrc;
            source.uid = m_plugin->dataSources[i].uid = ++DataPlugin::_uid;
            source.refreshmsec                        = settings.value(getSettingsCode(QUASAR_DP_REFRESH_PREFIX + source.key), m_plugin->dataSources[i].refreshMsec).toLongLong();
            source.enabled                            = settings.value(getSettingsCode(QUASAR_DP_ENABLED_PREFIX + source.key), true).toBool();

            // If data source is plugin signaled
            if (source.refreshmsec < 0)
            {
                source.locks = new DataLock;
                connect(this, &DataPlugin::dataReady, this, &DataPlugin::sendDataToSubscribers);
            }
        }
    }

    if (!m_plugin->init(this))
    {
        qWarning() << "Failed to initialize plugin" << m_libpath;
        return false;
    }

    if (m_code.isEmpty() || m_name.isEmpty())
    {
        qWarning() << "Invalid plugin name or code in file " << m_libpath;
        return false;
    }

    if (m_plugin->create_settings)
    {
        m_settings.reset(m_plugin->create_settings());

        if (m_settings)
        {
            // Fill saved settings if any
            auto it = m_settings->map.begin();

            while (it != m_settings->map.end())
            {
                switch (it->type)
                {
                    case QUASAR_SETTING_ENTRY_INT:
                        it->inttype.val = settings.value(getSettingsCode(it.key()), it->inttype.def).toInt();
                        break;

                    case QUASAR_SETTING_ENTRY_DOUBLE:
                        it->doubletype.val = settings.value(getSettingsCode(it.key()), it->doubletype.def).toDouble();
                        break;

                    case QUASAR_SETTING_ENTRY_BOOL:
                        it->booltype.val = settings.value(getSettingsCode(it.key()), it->booltype.def).toBool();
                        break;
                }

                ++it;
            }
        }
    }

    m_Initialized = true;
    return true;
}

bool DataPlugin::addSubscriber(QString source, QWebSocket* subscriber, QString widgetName)
{
    if (subscriber)
    {
        if (!m_datasources.contains(source))
        {
            qWarning() << "Unknown data source " << source << " requested in plugin " << m_code << " by widget " << widgetName;
            return false;
        }

        // TODO maybe needs locks
        DataSource& data = m_datasources[source];

        if (data.refreshmsec != 0)
        {
            data.subscribers << subscriber;

            if (data.refreshmsec > 0)
            {
                createTimer(data);
            }

            return true;
        }
        else
        {
            qWarning() << "Data source " << source << " in plugin " << m_code << " does not support subscriptions";
        }
    }
    else
    {
        qCritical() << "Unknown subscriber.";
    }

    return false;
}

void DataPlugin::removeSubscriber(QWebSocket* subscriber)
{
    // Removes subscriber from all data sources
    if (subscriber)
    {
        auto it = m_datasources.begin();

        while (it != m_datasources.end())
        {
            // Log if unsubscribed succeeded
            if (it->subscribers.remove(subscriber))
            {
                qInfo() << "Widget unsubscribed from plugin " << m_code << " data source " << it.key();
            }

            // Stop timer if no subscribers
            if (it->subscribers.isEmpty())
            {
                if (nullptr != it->timer)
                {
                    delete it->timer;
                    it->timer = nullptr;
                }
            }

            ++it;
        }
    }
}

void DataPlugin::pollAndSendData(QString source, QWebSocket* subscriber, QString widgetName)
{
    if (subscriber)
    {
        if (!m_datasources.contains(source))
        {
            qWarning() << "Unknown data source " << source << " requested in plugin " << m_code << " by widget " << widgetName;
            return;
        }

        // TODO maybe needs locks
        DataSource& data = m_datasources[source];

        QString message = craftDataMessage(data);

        if (!message.isEmpty())
        {
            subscriber->sendTextMessage(message);
        }
    }
}

void DataPlugin::sendDataToSubscribers(const DataSource& source)
{
    // TODO maybe needs locks

    // Only send if there are subscribers
    if (!source.subscribers.isEmpty())
    {
        QString message = craftDataMessage(source);

        if (!message.isEmpty())
        {
            for (QWebSocket* sub : qAsConst(source.subscribers))
            {
                sub->sendTextMessage(message);
            }
        }
    }

    // Signal data processed
    if (nullptr != source.locks)
    {
        {
            std::lock_guard<std::mutex> lk(source.locks->mutex);
            source.locks->processed = true;
        }

        source.locks->cv.notify_one();
    }
}

void DataPlugin::setDataSourceEnabled(QString source, bool enabled)
{
    if (!m_datasources.contains(source))
    {
        qWarning() << "Unknown data source " << source << " requested in plugin " << m_code;
        return;
    }

    DataSource& data = m_datasources[source];

    data.enabled = enabled;

    // Save to file
    QSettings settings;
    settings.setValue(getSettingsCode(QUASAR_DP_ENABLED_PREFIX + source), data.enabled);

    if (data.enabled && data.refreshmsec > 0)
    {
        // Create timer if not exist
        createTimer(data);
    }
    else if (nullptr != data.timer)
    {
        // Delete the timer if enabled
        delete data.timer;
        data.timer = nullptr;
    }
}

void DataPlugin::setDataSourceRefresh(QString source, int64_t msec)
{
    if (!m_datasources.contains(source))
    {
        qWarning() << "Unknown data source " << source << " requested in plugin " << m_code;
        return;
    }

    DataSource& data = m_datasources[source];

    data.refreshmsec = msec;

    // Save to file
    QSettings settings;
    settings.setValue(getSettingsCode(QUASAR_DP_REFRESH_PREFIX + source), data.refreshmsec);

    // Refresh timer if exists
    if (nullptr != data.timer)
    {
        data.timer->setInterval(data.refreshmsec);
    }
}

void DataPlugin::setCustomSetting(QString name, int val)
{
    if (m_settings)
    {
        m_settings->map[name].inttype.val = val;

        // Save to file
        QSettings settings;
        settings.setValue(getSettingsCode(name), val);
    }
}

void DataPlugin::setCustomSetting(QString name, double val)
{
    if (m_settings)
    {
        m_settings->map[name].doubletype.val = val;

        // Save to file
        QSettings settings;
        settings.setValue(getSettingsCode(name), val);
    }
}

void DataPlugin::setCustomSetting(QString name, bool val)
{
    if (m_settings)
    {
        m_settings->map[name].booltype.val = val;

        // Save to file
        QSettings settings;
        settings.setValue(getSettingsCode(name), val);
    }
}

void DataPlugin::updatePluginSettings()
{
    if (m_settings && m_plugin->update)
    {
        m_plugin->update(m_settings.get());
    }
}

void DataPlugin::emitDataReady(QString source)
{
    if (!m_datasources.contains(source))
    {
        qWarning() << "Unknown data source " << source << " requested in plugin " << m_code;
        return;
    }

    DataSource& data = m_datasources[source];

    if (nullptr != data.locks)
    {
        data.locks->ready = true;

        emit dataReady(data);
    }
}

void DataPlugin::waitDataProcessed(QString source)
{
    if (!m_datasources.contains(source))
    {
        qWarning() << "Unknown data source " << source << " requested in plugin " << m_code;
        return;
    }

    DataSource& data = m_datasources[source];

    if (nullptr != data.locks)
    {
        std::unique_lock<std::mutex> lk(data.locks->mutex);
        data.locks->cv.wait(lk, [&data] { return data.locks->processed; });

        data.locks->ready     = false;
        data.locks->processed = false;
    }
}

DataPlugin::DataPlugin(quasar_plugin_info_t* p, QString path, QObject* parent /*= Q_NULLPTR*/)
    : QObject(parent), m_plugin(p), m_libpath(path)
{
    if (nullptr == m_plugin)
    {
        throw std::invalid_argument("null plugin struct");
    }

    qRegisterMetaType<DataSource>("DataSource");
}

void DataPlugin::createTimer(DataSource& data)
{
    if (data.enabled && !data.timer)
    {
        // Initialize timer not done so
        data.timer = new QTimer(this);
        connect(data.timer, &QTimer::timeout, [this, &data] { sendDataToSubscribers(data); });

        data.timer->start(data.refreshmsec);
    }
}

QString DataPlugin::craftDataMessage(const DataSource& data)
{
    QJsonObject reply;

    // Poll plugin for data source
    if (!m_plugin->get_data(data.uid, &reply["data"]))
    {
        qWarning() << "getData(" << getCode() << ", " << data.key << ") failed";
        return QString();
    }


    // Craft response
    reply["type"]   = "data";
    reply["plugin"] = getCode();
    reply["source"] = data.key;

    QJsonDocument doc(reply);

    return QString(doc.toJson());
}
