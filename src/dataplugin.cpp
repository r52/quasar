#include "dataplugin.h"

#include <QDebug>
#include <QLibrary>
#include <QTimer>
#include <QtWebSockets/QWebSocket>
#include <QJsonDocument>
#include <QJsonObject>

unsigned int DataPlugin::_uid = 0;

void data_plugin_log(int level, const char* msg)
{
    switch (level)
    {
    case QUASAR_LOG_DEBUG:
        qDebug() << msg;
        break;
    case QUASAR_LOG_INFO:
    default:
        qInfo() << msg;
        break;
    case QUASAR_LOG_WARNING:
        qWarning() << msg;
        break;
    case QUASAR_LOG_CRITICAL:
        qCritical() << msg;
        break;
    }
}

DataPlugin::~DataPlugin()
{
    // Do some explicit cleanup
    for (DataSource& src : m_datasources)
    {
        if (nullptr != src.timer)
        {
            delete src.timer;
        }

        src.subscribers.clear();
    }

    if (nullptr != init)
    {
        init(QUASAR_INIT_SHUTDOWN, nullptr);
    }
}

DataPlugin* DataPlugin::load(QString libpath, QObject *parent /*= Q_NULLPTR*/)
{
    QLibrary lib(libpath);

    if (!lib.load())
    {
        qWarning() << lib.errorString();
        return nullptr;
    }

    plugin_free freeFunc = (plugin_free) lib.resolve("quasar_plugin_free");
    plugin_init initFunc = (plugin_init) lib.resolve("quasar_plugin_init");
    plugin_get_data getDataFunc = (plugin_get_data) lib.resolve("quasar_plugin_get_data");

    if (freeFunc && initFunc && getDataFunc)
    {
        DataPlugin* plugin = new DataPlugin(freeFunc, initFunc, getDataFunc, libpath, parent);
        return plugin;
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

    QuasarPluginInfo info;
    memset(&info, 0, sizeof(QuasarPluginInfo));

    info.logFunc = data_plugin_log;

    if (!init(QUASAR_INIT_START, &info))
    {
        qWarning() << "Failed to initialize QUASAR_INIT_START in plugin" << m_libpath;
        return false;
    }

    m_name = info.pluginName;
    m_code = info.pluginCode;
    m_author = info.author;
    m_desc = info.description;
    m_version = info.version;

    if (nullptr != info.dataSources)
    {
        for (unsigned int i = 0; i < info.numDataSources; i++)
        {
            if (m_datasources.contains(info.dataSources[i].dataSrc))
            {
                qWarning() << "Plugin " << m_code << " tried to register more than one data source '" << info.dataSources[i].dataSrc << "'";
                continue;
            }

            qInfo() << "Plugin " << m_code << " registering data source '" << info.dataSources[i].dataSrc << "'";

            DataSource& source = m_datasources[info.dataSources[i].dataSrc];
            source.key = info.dataSources[i].dataSrc;
            source.uid = info.dataSources[i].uid = ++DataPlugin::_uid;
            source.refreshmsec = info.dataSources[i].refreshMsec;
        }
    }

    if (!init(QUASAR_INIT_RESP, &info))
    {
        qWarning() << "Failed to initialize QUASAR_INIT_RESP in plugin" << m_libpath;
        return false;
    }

    if (m_code.isEmpty() || m_name.isEmpty())
    {
        qWarning() << "Invalid plugin name or code in file " << m_libpath;
        return false;
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

        if (0 == data.refreshmsec)
        {
            // Fire single shot
            QTimer::singleShot(0, this, [this, &data] { DataPlugin::getAndSendData(data); });
        }
        else if (!data.timer)
        {
            // Initialize timer not done so
            data.timer = new QTimer(this);
            connect(data.timer, &QTimer::timeout, this, [this, &data] { DataPlugin::getAndSendData(data); });

            data.timer->start(data.refreshmsec);
        }

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
        if (!getData(source.uid, buf, sizeof(buf), &datatype))
        {
            qWarning() << "getData(" << getCode() << ", " << source.key << ") failed";
            return;
        }

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

DataPlugin::DataPlugin(plugin_free freeFunc, plugin_init initFunc, plugin_get_data getDataFunc, QString path, QObject *parent /*= Q_NULLPTR*/) :
    QObject(parent), free(freeFunc), init(initFunc), getData(getDataFunc), m_libpath(path)
{
}
