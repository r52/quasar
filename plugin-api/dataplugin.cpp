#include "dataplugin.h"

#include <plugin_support_internal.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QLibrary>
#include <QSettings>
#include <QTimer>
#include <QtWebSockets/QWebSocket>

uintmax_t DataPlugin::_uid = 0;

DataPlugin::DataPlugin(quasar_plugin_info_t* p, plugin_destroy destroyfunc, QString path, QObject* parent /*= Q_NULLPTR*/)
    : QObject(parent), m_plugin(p), m_destroyfunc(destroyfunc), m_libpath(path)
{
    if (nullptr == m_plugin)
    {
        throw std::invalid_argument("null plugin struct");
    }

    m_name    = m_plugin->name;
    m_code    = m_plugin->code;
    m_author  = m_plugin->author;
    m_desc    = m_plugin->description;
    m_version = m_plugin->version;

    if (m_code.isEmpty() || m_name.isEmpty())
    {
        throw std::runtime_error("Invalid plugin name or code");
    }

    QSettings settings;

    // register data sources
    if (nullptr != m_plugin->dataSources)
    {
        for (unsigned int i = 0; i < m_plugin->numDataSources; i++)
        {
            if (m_datasources.count(m_plugin->dataSources[i].dataSrc))
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
                source.locks.reset(new DataLock);
                connect(this, &DataPlugin::dataReady, this, &DataPlugin::sendDataToSubscribersByName, Qt::QueuedConnection);
            }
        }
    }

    // create settings
    if (m_plugin->create_settings)
    {
        m_settings.reset(m_plugin->create_settings());

        if (m_settings)
        {
            // Fill saved settings if any
            auto it = m_settings->map.begin();

            while (it != m_settings->map.end())
            {
                switch (it->second.type)
                {
                    case QUASAR_SETTING_ENTRY_INT:
                        it->second.inttype.val = settings.value(getSettingsCode(it->first), it->second.inttype.def).toInt();
                        break;

                    case QUASAR_SETTING_ENTRY_DOUBLE:
                        it->second.doubletype.val = settings.value(getSettingsCode(it->first), it->second.doubletype.def).toDouble();
                        break;

                    case QUASAR_SETTING_ENTRY_BOOL:
                        it->second.booltype.val = settings.value(getSettingsCode(it->first), it->second.booltype.def).toBool();
                        break;
                }

                ++it;
            }

            updatePluginSettings();
        }
    }

    // initialize the plugin
    if (!m_plugin->init(this))
    {
        throw std::runtime_error("plugin init() failed");
    }
}

DataPlugin::~DataPlugin()
{
    if (nullptr != m_plugin->shutdown)
    {
        m_plugin->shutdown(this);
    }

    // Do some explicit cleanup
    for (auto& src : m_datasources)
    {
        src.second.timer.reset();
        src.second.locks.reset();

        src.second.subscribers.clear();
    }

    // plugin is responsible for cleanup of quasar_plugin_info_t*
    m_destroyfunc(m_plugin);
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

    plugin_load    loadfunc    = (plugin_load) lib.resolve("quasar_plugin_load");
    plugin_destroy destroyfunc = (plugin_destroy) lib.resolve("quasar_plugin_destroy");

    if (loadfunc && destroyfunc)
    {
        quasar_plugin_info_t* p = loadfunc();

        if (p && p->init && p->shutdown && p->get_data)
        {
            try
            {
                DataPlugin* plugin = new DataPlugin(p, destroyfunc, libpath, parent);
                return plugin;
            } catch (std::exception e)
            {
                qWarning() << "Exception: '" << e.what() << "' while initializing " << libpath;
            }
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

bool DataPlugin::addSubscriber(QString source, QWebSocket* subscriber, QString widgetName)
{
    if (subscriber)
    {
        if (!m_datasources.count(source))
        {
            qWarning() << "Unknown data source " << source << " requested in plugin " << m_code << " by widget " << widgetName;
            return false;
        }

        // TODO maybe needs locks
        DataSource& data = m_datasources[source];

        if (data.refreshmsec != 0)
        {
            data.subscribers.insert(subscriber);

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
            if (it->second.subscribers.erase(subscriber))
            {
                qInfo() << "Widget unsubscribed from plugin " << m_code << " data source " << it->first;
            }

            // Stop timer if no subscribers
            if (it->second.subscribers.empty())
            {
                it->second.timer.reset();
            }

            ++it;
        }
    }
}

void DataPlugin::pollAndSendData(QString source, QWebSocket* subscriber, QString widgetName)
{
    if (subscriber)
    {
        if (!m_datasources.count(source))
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
    if (!source.subscribers.empty())
    {
        QString message = craftDataMessage(source);

        if (!message.isEmpty())
        {
            for (auto sub : source.subscribers)
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
    if (!m_datasources.count(source))
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
    else if (data.timer)
    {
        // Delete the timer if enabled
        data.timer.reset();
    }
}

void DataPlugin::setDataSourceRefresh(QString source, int64_t msec)
{
    if (!m_datasources.count(source))
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
    if (!m_datasources.count(source))
    {
        qWarning() << "Unknown data source " << source << " requested in plugin " << m_code;
        return;
    }

    DataSource& data = m_datasources[source];

    if (data.locks)
    {
        emit dataReady(source);
    }
}

void DataPlugin::waitDataProcessed(QString source)
{
    if (!m_datasources.count(source))
    {
        qWarning() << "Unknown data source " << source << " requested in plugin " << m_code;
        return;
    }

    DataSource& data = m_datasources[source];

    if (data.locks)
    {
        std::unique_lock<std::mutex> lk(data.locks->mutex);
        data.locks->cv.wait(lk, [&data] { return data.locks->processed; });
        data.locks->processed = false;
    }
}

void DataPlugin::sendDataToSubscribersByName(QString source)
{
    if (!m_datasources.count(source))
    {
        qWarning() << "Unknown data source " << source << " requested in plugin " << m_code;
        return;
    }

    DataSource& data = m_datasources[source];

    sendDataToSubscribers(data);
}

void DataPlugin::createTimer(DataSource& data)
{
    if (data.enabled && !data.timer)
    {
        // Initialize timer not done so
        data.timer.reset(new QTimer(this));
        connect(data.timer.get(), &QTimer::timeout, [this, &data] { sendDataToSubscribers(data); });

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
