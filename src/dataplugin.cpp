#include "dataplugin.h"

#include <QDebug>
#include <QLibrary>
#include <QTimer>
#include <QtWebSockets/QWebSocket>
#include <QJsonDocument>
#include <QJsonObject>

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
}

DataPlugin* DataPlugin::load(QString libpath, QObject *parent /*= Q_NULLPTR*/)
{
    QLibrary lib(libpath);

    if (!lib.load())
    {
        qWarning() << lib.errorString();
        return nullptr;
    }

    plugin_free freeFunc = (plugin_free)lib.resolve("quasar_plugin_free");
    plugin_init initFunc = (plugin_init)lib.resolve("quasar_plugin_init");
    plugin_get_data getDataFunc = (plugin_get_data)lib.resolve("quasar_plugin_get_data");

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

    if (!init(&info))
    {
        qWarning() << "Failed to initialize plugin" << m_libpath;
        return false;
    }

    m_name = info.pluginName;
    m_code = info.pluginCode;
    m_author = info.author;
    m_desc = info.description;

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
            m_datasources[info.dataSources[i].dataSrc].refreshmsec = info.dataSources[i].refreshMsec;
        }

        free(info.dataSources, info.numDataSources);
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

        // Initialize timer not done so
        if (!data.timer)
        {
            data.timer = new QTimer(this);
            connect(data.timer, &QTimer::timeout, this, [this, source] { DataPlugin::getAndSendData(source); });

            if (0 == data.refreshmsec)
            {
                data.timer->setSingleShot(true);
            }

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
        for (DataSource& src : m_datasources)
        {
            src.subscribers.remove(subscriber);
        }
    }
}

void DataPlugin::getAndSendData(QString source)
{
    // TODO maybe needs locks
    if (!m_datasources.contains(source))
    {
        qWarning() << "Unknown data source '" << source << "' requested in plugin '" << m_code << "'";
        return;
    }

    DataSource& data = m_datasources[source];

    // Only send if there are subscribers
    if (data.subscribers.count() > 0)
    {
        char buf[1024] = "";
        int convert_to_json = false;

        // Poll plugin for data source
        if (!getData(qPrintable(source), buf, sizeof(buf), &convert_to_json))
        {
            qWarning() << "getData(" << getCode() << ", " << source << ") failed";
            return;
        }

        // Craft response
        QJsonObject reply;
        reply["type"] = "data";
        reply["plugin"] = getCode();
        reply["source"] = source;

        if (convert_to_json)
        {
            QString str = QString::fromUtf8(buf);
            reply["data"] = QJsonDocument::fromJson(str.toUtf8()).object();
        }
        else
        {
            reply["data"] = QString::fromUtf8(buf);
        }

        QJsonDocument doc(reply);
        QString message(doc.toJson());

        for (QWebSocket *sub : qAsConst(data.subscribers))
        {
            sub->sendTextMessage(message);
        }
    }
}

DataPlugin::DataPlugin(plugin_free freeFunc, plugin_init initFunc, plugin_get_data getDataFunc, QString path, QObject *parent /*= Q_NULLPTR*/) :
    QObject(parent), free(freeFunc), init(initFunc), getData(getDataFunc), m_libpath(path)
{
}
