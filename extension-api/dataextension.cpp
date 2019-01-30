#include "dataextension.h"

#include <extension_support_internal.h>
#include <unordered_set>

#include <QJsonDocument>
#include <QJsonObject>
#include <QLibrary>
#include <QSettings>
#include <QTimer>
#include <QtWebSockets/QWebSocket>

//! Ensure c strings are null terminated and converted to a utf8 QString
#define CHAR_TO_UTF8(d, x) \
    x[sizeof(x) - 1] = 0;  \
    d                = QString::fromUtf8(x);

uintmax_t DataExtension::_uid = 0;

DataExtension::DataExtension(quasar_ext_info_t* p, extension_destroy destroyfunc, QString path, QObject* parent /*= Q_NULLPTR*/)
    : QObject(parent), m_extension(p), m_destroyfunc(destroyfunc), m_libpath(path)
{
    if (nullptr == m_extension)
    {
        throw std::invalid_argument("null extension struct");
    }

    CHAR_TO_UTF8(m_name, m_extension->name);
    CHAR_TO_UTF8(m_code, m_extension->code);
    CHAR_TO_UTF8(m_author, m_extension->author);
    CHAR_TO_UTF8(m_desc, m_extension->description);
    CHAR_TO_UTF8(m_version, m_extension->version);

    if (m_code.isEmpty() || m_name.isEmpty())
    {
        throw std::runtime_error("Invalid extension name or code");
    }

    QSettings settings;

    // register data sources
    if (nullptr != m_extension->dataSources)
    {
        for (unsigned int i = 0; i < m_extension->numDataSources; i++)
        {
            CHAR_TO_UTF8(QString srcname, m_extension->dataSources[i].dataSrc);

            if (m_datasources.count(srcname))
            {
                qWarning() << "Extension " << m_code << " tried to register more than one data source '" << srcname << "'";
                continue;
            }

            qInfo() << "Extension " << m_code << " registering data source '" << srcname << "'";

            DataSource& source = m_datasources[srcname];
            source.key         = srcname;
            source.uid = m_extension->dataSources[i].uid = ++DataExtension::_uid;
            source.refreshmsec                           = settings.value(getSettingsCode(QUASAR_DP_REFRESH_PREFIX + source.key), (qlonglong) m_extension->dataSources[i].refreshMsec).toLongLong();
            source.enabled                               = settings.value(getSettingsCode(QUASAR_DP_ENABLED_PREFIX + source.key), true).toBool();

            // If data source is extension signaled or async poll
            if (source.refreshmsec <= 0)
            {
                source.locks = std::make_unique<DataLock>();
                connect(this, &DataExtension::dataReady, this, &DataExtension::sendDataToSubscribersByName, Qt::QueuedConnection);
            }
        }
    }

    // create settings
    if (m_extension->create_settings)
    {
        m_settings.reset(m_extension->create_settings());

        if (m_settings)
        {
            // Fill saved settings if any
            for (auto& it : m_settings->map)
            {
                switch (it.second.type)
                {
                    case QUASAR_SETTING_ENTRY_INT:
                        it.second.inttype.val = settings.value(getSettingsCode(it.first), it.second.inttype.def).toInt();
                        break;

                    case QUASAR_SETTING_ENTRY_DOUBLE:
                        it.second.doubletype.val = settings.value(getSettingsCode(it.first), it.second.doubletype.def).toDouble();
                        break;

                    case QUASAR_SETTING_ENTRY_BOOL:
                        it.second.booltype.val = settings.value(getSettingsCode(it.first), it.second.booltype.def).toBool();
                        break;
                }
            }

            updateExtensionSettings();
        }
    }

    // initialize the extension
    if (!m_extension->init(this))
    {
        throw std::runtime_error("extension init() failed");
    }
}

DataExtension::~DataExtension()
{
    if (nullptr != m_extension->shutdown)
    {
        m_extension->shutdown(this);
    }

    // Do some explicit cleanup
    for (auto& src : m_datasources)
    {
        src.second.timer.reset();
        src.second.locks.reset();

        src.second.subscribers.clear();
    }

    // extension is responsible for cleanup of quasar_ext_info_t*
    m_destroyfunc(m_extension);
    m_extension = nullptr;
}

DataExtension* DataExtension::load(QString libpath, QObject* parent /*= Q_NULLPTR*/)
{
    QLibrary lib(libpath);

    if (!lib.load())
    {
        qWarning() << lib.errorString();
        return nullptr;
    }

    extension_load    loadfunc    = (extension_load) lib.resolve("quasar_ext_load");
    extension_destroy destroyfunc = (extension_destroy) lib.resolve("quasar_ext_destroy");

    if (!loadfunc || !destroyfunc)
    {
        qWarning() << "Failed to resolve extension API in" << libpath;
        return nullptr;
    }

    quasar_ext_info_t* p = loadfunc();

    if (!p || !p->init || !p->shutdown || !p->get_data)
    {
        qWarning() << "quasar_ext_load failed in" << libpath;
        return nullptr;
    }

    try
    {
        DataExtension* extension = new DataExtension(p, destroyfunc, libpath, parent);
        return extension;
    } catch (std::exception e)
    {
        qWarning() << "Exception: '" << e.what() << "' while initializing " << libpath;
    }

    return nullptr;
}

bool DataExtension::addSubscriber(QString source, QWebSocket* subscriber, QString widgetName)
{
    if (!subscriber)
    {
        qCritical() << "Unknown subscriber.";
        return false;
    }

    if (!m_datasources.count(source))
    {
        qWarning() << "Unknown data source " << source << " requested in extension " << m_code << " by widget " << widgetName;
        return false;
    }

    // TODO maybe needs locks
    DataSource& data = m_datasources[source];

    data.subscribers.insert(subscriber);

    if (data.refreshmsec > 0)
    {
        createTimer(data);
    }

    // Send settings if applicable
    auto payload = craftSettingsMessage();

    if (!payload.isEmpty())
    {
        subscriber->sendTextMessage(payload);
    }

    return true;
}

void DataExtension::removeSubscriber(QWebSocket* subscriber)
{
    if (!subscriber)
    {
        qWarning() << "Null subscriber";
        return;
    }

    // Removes subscriber from all data sources
    for (auto& it : m_datasources)
    {
        // Log if unsubscribed succeeded
        if (it.second.subscribers.erase(subscriber))
        {
            qInfo() << "Widget unsubscribed from extension " << m_code << " data source " << it.first;
        }

        // Stop timer if no subscribers
        if (it.second.subscribers.empty())
        {
            it.second.timer.reset();
        }
    }
}

void DataExtension::pollAndSendData(QString source, QWebSocket* subscriber, QString widgetName)
{
    if (!subscriber)
    {
        qWarning() << "Null subscriber";
        return;
    }

    if (!m_datasources.count(source))
    {
        qWarning() << "Unknown data source " << source << " requested in extension " << m_code << " by widget " << widgetName;
        return;
    }

    // TODO maybe needs locks
    DataSource& data = m_datasources[source];

    QString message = craftDataMessage(data);

    if (!message.isEmpty())
    {
        subscriber->sendTextMessage(message);

        // Pop client from poll queue if data was readily available
        data.subscribers.erase(subscriber);
    }
}

void DataExtension::sendDataToSubscribers(DataSource& source)
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

    if (source.refreshmsec == 0)
    {
        // Clear poll queue
        source.subscribers.clear();
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

void DataExtension::setDataSourceEnabled(QString source, bool enabled)
{
    if (!m_datasources.count(source))
    {
        qWarning() << "Unknown data source " << source << " requested in extension " << m_code;
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

void DataExtension::setDataSourceRefresh(QString source, int64_t msec)
{
    if (!m_datasources.count(source))
    {
        qWarning() << "Unknown data source " << source << " requested in extension " << m_code;
        return;
    }

    DataSource& data = m_datasources[source];

    data.refreshmsec = msec;

    // Save to file
    QSettings settings;
    settings.setValue(getSettingsCode(QUASAR_DP_REFRESH_PREFIX + source), (qlonglong) data.refreshmsec);

    // Refresh timer if exists
    if (nullptr != data.timer)
    {
        data.timer->setInterval(data.refreshmsec);
    }
}

void DataExtension::setCustomSetting(QString name, int val)
{
    if (m_settings)
    {
        m_settings->map[name].inttype.val = val;

        // Save to file
        QSettings settings;
        settings.setValue(getSettingsCode(name), val);
    }
}

void DataExtension::setCustomSetting(QString name, double val)
{
    if (m_settings)
    {
        m_settings->map[name].doubletype.val = val;

        // Save to file
        QSettings settings;
        settings.setValue(getSettingsCode(name), val);
    }
}

void DataExtension::setCustomSetting(QString name, bool val)
{
    if (m_settings)
    {
        m_settings->map[name].booltype.val = val;

        // Save to file
        QSettings settings;
        settings.setValue(getSettingsCode(name), val);
    }
}

void DataExtension::updateExtensionSettings()
{
    if (m_settings && m_extension->update)
    {
        m_extension->update(m_settings.get());

        propagateSettingsToAllUniqueSubscribers();
    }
}

void DataExtension::emitDataReady(QString source)
{
    if (!m_datasources.count(source))
    {
        qWarning() << "Unknown data source " << source << " requested in extension " << m_code;
        return;
    }

    DataSource& data = m_datasources[source];

    if (data.locks)
    {
        emit dataReady(source);
    }
}

void DataExtension::waitDataProcessed(QString source)
{
    if (!m_datasources.count(source))
    {
        qWarning() << "Unknown data source " << source << " requested in extension " << m_code;
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

void DataExtension::sendDataToSubscribersByName(QString source)
{
    if (!m_datasources.count(source))
    {
        qWarning() << "Unknown data source " << source << " requested in extension " << m_code;
        return;
    }

    DataSource& data = m_datasources[source];

    sendDataToSubscribers(data);
}

void DataExtension::createTimer(DataSource& data)
{
    if (data.enabled && !data.timer)
    {
        // Initialize timer not done so
        data.timer = std::make_unique<QTimer>(this);
        connect(data.timer.get(), &QTimer::timeout, [this, &data] { sendDataToSubscribers(data); });

        data.timer->start(data.refreshmsec);
    }
}

QString DataExtension::craftDataMessage(const DataSource& data)
{
    QJsonObject src;
    auto        dat = src[data.key];

    // Poll extension for data source
    if (!m_extension->get_data(data.uid, &dat))
    {
        qWarning() << "getData(" << getCode() << ", " << data.key << ") failed";
        return QString();
    }

    if (dat.isNull())
    {
        // Allow empty return (for async data)
        return QString();
    }

    // Craft response
    QJsonObject ext;
    QJsonObject reply;
    ext[getCode()] = src;
    reply["data"]  = ext;

    QJsonDocument doc(reply);

    return QString::fromUtf8(doc.toJson());
}

QString DataExtension::craftSettingsMessage()
{
    QString payload;

    if (m_settings)
    {
        // TODO: FIX
        // Craft the settings message
        QJsonObject msg;
        QJsonObject settings;

        msg["type"]      = "settings";
        msg["extension"] = getCode();

        for (auto& s : m_settings->map)
        {
            switch (s.second.type)
            {
                case QUASAR_SETTING_ENTRY_INT:
                    settings[s.first] = s.second.inttype.val;
                    break;

                case QUASAR_SETTING_ENTRY_DOUBLE:
                    settings[s.first] = s.second.doubletype.val;
                    break;

                case QUASAR_SETTING_ENTRY_BOOL:
                    settings[s.first] = s.second.booltype.val;
                    break;
            }
        }

        msg["data"] = settings;
        QJsonDocument doc(msg);

        payload = QString::fromUtf8(doc.toJson());
    }

    return payload;
}

void DataExtension::propagateSettingsToAllUniqueSubscribers()
{
    if (m_settings)
    {
        std::unordered_set<QWebSocket*> unique_subs;

        // Collect unique subscribers
        for (auto& source : m_datasources)
        {
            unique_subs.insert(source.second.subscribers.begin(), source.second.subscribers.end());
        }

        auto payload = craftSettingsMessage();

        if (!payload.isEmpty())
        {
            // Send the payload
            for (auto sub : unique_subs)
            {
                sub->sendTextMessage(payload);
            }
        }
    }
}
