#include "dataplugin.h"

#include <plugin_support_internal.h>

#include <QSettings>
#include <QLibrary>
#include <QTimer>
#include <QtWebSockets/QWebSocket>
#include <QJsonDocument>
#include <QJsonObject>

uintmax_t DataPlugin::_uid = 0;

DataPlugin::~DataPlugin()
{
    // Do some explicit cleanup
    for (DataSource& src : m_datasources)
    {
        if (nullptr != src.timer)
        {
            delete src.timer;
            src.timer = nullptr;
        }

        src.subscribers.clear();
    }

    if (nullptr != m_plugin->shutdown)
    {
        m_plugin->shutdown(m_plugin);
    }

    // plugin is responsible for cleanup of quasar_plugin_info_t*
    m_plugin = nullptr;
}

DataPlugin* DataPlugin::load(QString libpath, QObject *parent /*= Q_NULLPTR*/)
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

    m_name = m_plugin->name;
    m_code = m_plugin->code;
    m_author = m_plugin->author;
    m_desc = m_plugin->description;
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
            source.key = m_plugin->dataSources[i].dataSrc;
            source.uid = m_plugin->dataSources[i].uid = ++DataPlugin::_uid;
            source.refreshmsec = settings.value(getSettingsCode(QUASAR_DP_REFRESH_PREFIX + source.key), m_plugin->dataSources[i].refreshMsec).toUInt();
        }
    }

    if (!m_plugin->init(m_plugin))
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

bool DataPlugin::addSubscriber(QString source, QWebSocket *subscriber, QString widgetName)
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

        data.subscribers << subscriber;

        createTimer(data);

        return true;
    }

    qCritical() << "Unknown subscriber.";

    return false;
}

void DataPlugin::removeSubscriber(QWebSocket *subscriber)
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

void DataPlugin::getAndSendData(DataSource& source)
{
    // TODO maybe needs locks

    // Only send if there are subscribers
    if (!source.subscribers.isEmpty())
    {
        char buf[1024] = "";
        int datatype = QUASAR_TREAT_AS_STRING;

        // Poll plugin for data source
        if (!m_plugin->get_data(source.uid, buf, sizeof(buf), &datatype))
        {
            qWarning() << "getData(" << getCode() << ", " << source.key << ") failed";
            return;
        }

        // Make sure it is null terminated
        buf[sizeof(buf) - 1] = 0;

        // Craft response
        QJsonObject reply;
        reply["type"] = "data";
        reply["plugin"] = getCode();
        reply["source"] = source.key;

        switch (datatype)
        {
            case QUASAR_TREAT_AS_STRING:
            {
                reply["data"] = QString::fromUtf8(buf);
                break;
            }
            case QUASAR_TREAT_AS_JSON:
            {
                QString str = QString::fromUtf8(buf);
                reply["data"] = QJsonDocument::fromJson(str.toUtf8()).object();
                break;
            }
            case QUASAR_TREAT_AS_BINARY:
            {
                reply["data"] = QJsonDocument::fromRawData(buf, sizeof(buf)).object();
                break;
            }
            default:
            {
                qWarning() << "Undefined return data type " << datatype;
                break;
            }
        }

        QJsonDocument doc(reply);
        QString message(doc.toJson());

        for (QWebSocket *sub : qAsConst(source.subscribers))
        {
            sub->sendTextMessage(message);
        }
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

    // Save to file
    QSettings settings;
    settings.setValue(getSettingsCode(QUASAR_DP_ENABLED_PREFIX + source), enabled);

    if (enabled)
    {
        // Create timer if not enabled
        createTimer(data);
    }
    else
    {
        // Delete the timer if enabled
        if (nullptr != data.timer)
        {
            delete data.timer;
            data.timer = nullptr;
        }
    }
}

void DataPlugin::setDataSourceRefresh(QString source, uint32_t msec)
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

DataPlugin::DataPlugin(quasar_plugin_info_t* p, QString path, QObject *parent /*= Q_NULLPTR*/) :
    QObject(parent), m_plugin(p), m_libpath(path)
{
    if (nullptr == m_plugin)
    {
        throw std::invalid_argument("null plugin struct");
    }
}

void DataPlugin::createTimer(DataSource& data)
{
    QSettings settings;
    bool timerEnabled = settings.value(getSettingsCode(QUASAR_DP_ENABLED_PREFIX + data.key), true).toBool();

    if (timerEnabled)
    {
        if (0 == data.refreshmsec)
        {
            // Fire single shot
            QTimer::singleShot(0, this, [this, &data] { DataPlugin::getAndSendData(data); });
        }
        else if (!data.timer)
        {
            // Initialize timer not done so
            data.timer = new QTimer(this);
            connect(data.timer, &QTimer::timeout, [this, &data] { DataPlugin::getAndSendData(data); });

            data.timer->start(data.refreshmsec);
        }
    }
}
