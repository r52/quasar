#include "dataextension.h"

#include <extension_support_internal.h>
#include <unordered_set>

#include <QJsonArray>
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

DataExtension::DataExtension(quasar_ext_info_t* p, extension_destroy destroyfunc, QString path, QObject* parent) :
    QObject(parent),
    m_extension(p),
    m_destroyfunc(destroyfunc),
    m_libpath(path)
{
    if (nullptr == m_extension)
    {
        throw std::invalid_argument("null extension info struct");
    }

    // Currently only support latest API version
    if (m_extension->api_version != QUASAR_API_VERSION)
    {
        throw std::invalid_argument("unsupported API version");
    }

    if (nullptr == m_extension->fields)
    {
        throw std::invalid_argument("null extension fields struct");
    }

    CHAR_TO_UTF8(m_name, m_extension->fields->name);
    CHAR_TO_UTF8(m_fullname, m_extension->fields->fullname);
    CHAR_TO_UTF8(m_author, m_extension->fields->author);
    CHAR_TO_UTF8(m_desc, m_extension->fields->description);
    CHAR_TO_UTF8(m_version, m_extension->fields->version);
    CHAR_TO_UTF8(m_url, m_extension->fields->url);

    if (m_name.isEmpty() || m_fullname.isEmpty())
    {
        throw std::runtime_error("Invalid extension identifier or name");
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
                qWarning() << "Extension " << m_name << " tried to register more than one data source '" << srcname << "'";
                continue;
            }

            qInfo() << "Extension " << m_name << " registering data source '" << srcname << "'";

            DataSource& source = m_datasources[srcname];
            source.name        = srcname;
            source.uid = m_extension->dataSources[i].uid = ++DataExtension::_uid;
            source.refreshmsec =
                settings.value(getSettingsKey(source.name + QUASAR_DP_RATE_PREFIX), (qlonglong) m_extension->dataSources[i].refreshMsec).toLongLong();
            source.enabled = settings.value(getSettingsKey(source.name + QUASAR_DP_ENABLED), true).toBool();

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
                        it.second.inttype.val = settings.value(getSettingsKey(it.first), it.second.inttype.def).toInt();
                        break;

                    case QUASAR_SETTING_ENTRY_DOUBLE:
                        it.second.doubletype.val = settings.value(getSettingsKey(it.first), it.second.doubletype.def).toDouble();
                        break;

                    case QUASAR_SETTING_ENTRY_BOOL:
                        it.second.booltype.val = settings.value(getSettingsKey(it.first), it.second.booltype.def).toBool();
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

DataExtension* DataExtension::load(QString libpath, QObject* parent)
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
        qWarning() << "Unknown data source " << source << " requested in extension " << m_name << " by widget " << widgetName;
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
            qInfo() << "Widget unsubscribed from extension " << m_name << " data source " << it.first;
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
        qWarning() << "Unknown data source " << source << " requested in extension " << m_name << " by widget " << widgetName;
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

QJsonObject DataExtension::getMetadataJSON(bool settings_only)
{
    QSettings   settings;
    QJsonObject mdat;

    if (!settings_only)
    {
        // ext info
        mdat["name"]        = getName();
        mdat["fullname"]    = getFullName();
        mdat["version"]     = getVersion();
        mdat["author"]      = getAuthor();
        mdat["description"] = getDesc();
        mdat["url"]         = getUrl();
    }

    // ext rates
    QJsonArray rates;

    for (auto& isrc : m_datasources)
    {
        QJsonObject rate;

        auto& src       = isrc.second;
        rate["name"]    = src.name;
        rate["enabled"] = settings.value(getSettingsKey(src.name + QUASAR_DP_ENABLED), true).toBool();
        rate["rate"]    = src.refreshmsec;

        rates.append(rate);
    }

    mdat["rates"] = rates;

    // ext settings
    if (m_settings)
    {
        static const QString entryTypeStrArr[] = {"int", "double", "bool"};

        QJsonArray extsettings;

        for (auto& it : m_settings->map)
        {
            QJsonObject s;

            auto& entry = it.second;
            s["name"]   = it.first;
            s["desc"]   = entry.description;
            s["type"]   = entryTypeStrArr[entry.type];

            switch (entry.type)
            {
                case QUASAR_SETTING_ENTRY_INT:
                {
                    s["min"]  = entry.inttype.min;
                    s["max"]  = entry.inttype.max;
                    s["step"] = entry.inttype.step;
                    s["def"]  = entry.inttype.def;
                    s["val"]  = entry.inttype.val;
                    break;
                }

                case QUASAR_SETTING_ENTRY_DOUBLE:
                {
                    s["min"]  = entry.doubletype.min;
                    s["max"]  = entry.doubletype.max;
                    s["step"] = entry.doubletype.step;
                    s["def"]  = entry.doubletype.def;
                    s["val"]  = entry.doubletype.val;
                    break;
                }

                case QUASAR_SETTING_ENTRY_BOOL:
                {
                    s["def"] = entry.booltype.def;
                    s["val"] = entry.booltype.val;
                    break;
                }
            }

            extsettings.append(s);
        }

        mdat["settings"] = extsettings;
    }
    else
    {
        mdat["settings"] = QJsonValue(QJsonValue::Null);
    }

    return mdat;
}

void DataExtension::setDataSourceEnabled(QString source, bool enabled)
{
    if (!m_datasources.count(source))
    {
        qWarning() << "Unknown data source " << source << " requested in extension " << m_name;
        return;
    }

    DataSource& data = m_datasources[source];

    data.enabled = enabled;

    // Save to file
    QSettings settings;
    settings.setValue(getSettingsKey(source + QUASAR_DP_ENABLED), data.enabled);

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
        qWarning() << "Unknown data source " << source << " requested in extension " << m_name;
        return;
    }

    DataSource& data = m_datasources[source];

    data.refreshmsec = msec;

    // Save to file
    QSettings settings;
    settings.setValue(getSettingsKey(source + QUASAR_DP_RATE_PREFIX), (qlonglong) data.refreshmsec);

    // Refresh timer if exists
    if (nullptr != data.timer)
    {
        data.timer->setInterval(data.refreshmsec);
    }
}

void DataExtension::setAllSettings(const QJsonObject& setjs)
{
    QSettings settings;

    for (auto& setkey : setjs.keys())
    {
        auto parts = setkey.split("/", QString::SkipEmptyParts);

        if (parts.length() < 2 || parts.length() > 3)
        {
            // invalid key
            qWarning() << "Invalid setting key " << setkey;
            continue;
        }

        auto name = parts[1];

        if (parts.length() < 3 && m_settings)
        {
            // custom setting
            // <extname>/<setting name>

            auto& cmap = m_settings->map;
            auto& cset = cmap[name];

            switch (cset.type)
            {
                case QUASAR_SETTING_ENTRY_INT:
                    cset.inttype.val = setjs[setkey].toInt();
                    settings.setValue(getSettingsKey(name), setjs[setkey].toInt());
                    break;
                case QUASAR_SETTING_ENTRY_DOUBLE:
                    cset.doubletype.val = setjs[setkey].toDouble();
                    settings.setValue(getSettingsKey(name), setjs[setkey].toDouble());
                    break;
                case QUASAR_SETTING_ENTRY_BOOL:
                    cset.booltype.val = setjs[setkey].toBool();
                    settings.setValue(getSettingsKey(name), setjs[setkey].toBool());
                    break;
            }
        }
        else
        {
            // <extname>/<source name>/<rate|enabled>

            auto type = parts[2];
            if (type == "rate")
            {
                setDataSourceRefresh(name, setjs[setkey].toInt());
            }
            else if (type == "enabled")
            {
                setDataSourceEnabled(name, setjs[setkey].toBool());
            }
        }
    }

    updateExtensionSettings();
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
        qWarning() << "Unknown data source " << source << " requested in extension " << m_name;
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
        qWarning() << "Unknown data source " << source << " requested in extension " << m_name;
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
        qWarning() << "Unknown data source " << source << " requested in extension " << m_name;
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
    auto        dat = src[data.name];

    // Poll extension for data source
    if (!m_extension->get_data(data.uid, &dat))
    {
        qWarning() << "get_data(" << m_name << ", " << data.name << ") failed";
        return QString();
    }

    if (dat.isUndefined())
    {
        // Allow empty return (for async data)
        return QString();
    }

    // Craft response
    auto msg = QJsonObject{{"data", QJsonObject{{m_name, src}}}};

    QJsonDocument doc(msg);

    return QString::fromUtf8(doc.toJson());
}

QString DataExtension::craftSettingsMessage()
{
    QString payload;

    if (m_settings)
    {
        auto mdat = getMetadataJSON(true);
        auto msg  = QJsonObject{{"data", QJsonObject{{"settings", QJsonObject{{m_name, mdat}}}}}};

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
