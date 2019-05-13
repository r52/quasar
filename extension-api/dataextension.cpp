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
    QObject(parent), m_extension(p), m_destroyfunc(destroyfunc), m_libpath(path), m_initialized(false)
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
            CHAR_TO_UTF8(QString srcname, m_extension->dataSources[i].name);

            if (m_datasources.count(srcname))
            {
                qWarning() << "Extension" << m_name << "tried to register more than one data source" << srcname;
                continue;
            }

            qInfo() << "Extension" << m_name << "registering data source" << srcname;

            DataSource& source = m_datasources[srcname];
            source.enabled     = settings.value(getSettingsKey(source.name + QUASAR_DP_ENABLED), true).toBool();
            source.name        = srcname;
            source.uid = m_extension->dataSources[i].uid = ++DataExtension::_uid;
            source.rate      = settings.value(getSettingsKey(source.name + QUASAR_DP_RATE_PREFIX), (qlonglong) m_extension->dataSources[i].rate).toLongLong();
            source.validtime = m_extension->dataSources[i].validtime;

            // Initialize type specific fields
            if (source.rate == QUASAR_POLLING_SIGNALED)
            {
                source.locks = std::make_unique<DataLock>();
            }

            if (source.rate == QUASAR_POLLING_CLIENT)
            {
                source.expiry = std::chrono::system_clock::now();
            }

            // If data source is extension signaled or async poll
            if (source.rate <= QUASAR_POLLING_CLIENT)
            {
                connect(this, &DataExtension::dataReady, this, &DataExtension::handleDataReadySignal, Qt::QueuedConnection);
            }
        }
    }

    // create settings
    if (m_extension->create_settings)
    {
        m_settings.reset(m_extension->create_settings());

        if (!m_settings)
        {
            throw std::runtime_error("extension create_settings() failed");
        }

        // Fill saved settings if any
        for (auto it = m_settings->map.begin(); it != m_settings->map.end(); ++it)
        {
            auto& def = it.value();

            switch (def.type)
            {
                case QUASAR_SETTING_ENTRY_INT:
                {
                    auto c = def.var.value<esi_inttype_t>();
                    c.val  = settings.value(getSettingsKey(it.key()), c.def).toInt();
                    def.var.setValue(c);
                    break;
                }

                case QUASAR_SETTING_ENTRY_DOUBLE:
                {
                    auto c = def.var.value<esi_doubletype_t>();
                    c.val  = settings.value(getSettingsKey(it.key()), c.def).toDouble();
                    def.var.setValue(c);
                    break;
                }

                case QUASAR_SETTING_ENTRY_BOOL:
                {
                    auto c = def.var.value<esi_doubletype_t>();
                    c.val  = settings.value(getSettingsKey(it.key()), c.def).toBool();
                    def.var.setValue(c);
                    break;
                }

                case QUASAR_SETTING_ENTRY_STRING:
                {
                    auto c = def.var.value<esi_stringtype_t>();
                    c.val  = settings.value(getSettingsKey(it.key()), c.def).toString();
                    def.var.setValue(c);
                    break;
                }

                case QUASAR_SETTING_ENTRY_SELECTION:
                {
                    auto c = def.var.value<quasar_selection_options_t>();
                    c.val  = settings.value(getSettingsKey(it.key()), c.list.at(0).value).toString();
                    def.var.setValue(c);
                    break;
                }
            }
        }

        updateExtensionSettings();
    }
}

DataExtension::~DataExtension()
{
    if (nullptr != m_extension->shutdown)
    {
        m_extension->shutdown(this);
    }

    // Do some explicit cleanup
    for (auto it = m_datasources.begin(); it != m_datasources.end(); ++it)
    {
        it.value().timer.reset();
        it.value().locks.reset();

        it.value().subscribers.clear();
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

    if (!p || !p->init || !p->shutdown || !p->get_data || !p->fields || !p->dataSources)
    {
        qWarning() << "quasar_ext_load failed in" << libpath << ": required extension data missing";
        return nullptr;
    }

    try
    {
        DataExtension* extension = new DataExtension(p, destroyfunc, libpath, parent);
        return extension;
    } catch (std::exception e)
    {
        qWarning() << "Exception:" << e.what() << "while initializing" << libpath;
    }

    return nullptr;
}

void DataExtension::initialize()
{
    if (!m_initialized)
    {
        // initialize the extension
        if (!m_extension->init(this))
        {
            throw std::runtime_error("extension init() failed");
        }

        if (m_settings)
        {
            updateExtensionSettings();
        }

        m_initialized = true;

        qInfo() << "Extension" << getName() << "initialized.";
    }
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
        qWarning() << "Unknown data source" << source << "requested in extension" << m_name << "by widget" << widgetName;
        return false;
    }

    // XXX maybe needs locks
    DataSource& dsrc = m_datasources[source];

    if (dsrc.rate == QUASAR_POLLING_CLIENT)
    {
        qWarning() << "Polling data source" << source << "in extension" << m_name << "requested by widget" << widgetName << "does not accept subscribers";
        return false;
    }

    dsrc.subscribers.insert(subscriber);

    if (dsrc.rate > QUASAR_POLLING_CLIENT)
    {
        createTimer(dsrc);
    }

    // Send settings if applicable
    auto payload = craftSettingsMessage();

    if (!payload.isEmpty())
    {
        QMetaObject::invokeMethod(subscriber, [=] { subscriber->sendTextMessage(payload); });
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

    for (auto it = m_datasources.begin(); it != m_datasources.end(); ++it)
    {
        // Log if unsubscribed succeeded
        if (it.value().subscribers.erase(subscriber))
        {
            qInfo() << "Widget unsubscribed from extension" << m_name << "data source" << it.key();
        }

        // Stop timer if no subscribers
        if (it.value().subscribers.empty())
        {
            it.value().timer.reset();
        }
    }
}

void DataExtension::pollAndSendData(QString source, QString args, QWebSocket* client, QString widgetName)
{
    if (!client)
    {
        qWarning() << "Null client";
        return;
    }

    QStringList dlist = source.split(',', QString::SkipEmptyParts);

    QJsonObject data;
    QJsonArray  errs;

    for (QString& src : dlist)
    {
        if (!m_datasources.count(src))
        {
            QString m = "Unknown data source " + src + " requested in extension " + m_name + " by widget " + widgetName;
            errs.append(m);
            qWarning().noquote() << m;
            continue;
        }

        // XXX maybe needs locks
        DataSource& dsrc   = m_datasources[src];
        auto        result = getDataFromSource(data, errs, dsrc, args);

        switch (result)
        {
            case GET_DATA_FAILED:
            {
                QString m = "getDataFromSource(" + src + ") failed in extension " + m_name + " requested by widget " + widgetName;
                errs.append(m);
                qWarning().noquote() << m;
            }
            break;
            case GET_DATA_DELAYED:
                // add to poll queue
                dsrc.pollqueue.insert(client);
                break;
            case GET_DATA_SUCCESS:
                // done. do nothing
                break;
        }
    }

    QString message = craftDataMessage(data, errs);

    if (!message.isEmpty())
    {
        QMetaObject::invokeMethod(client, [=] { client->sendTextMessage(message); });
    }
}

void DataExtension::sendDataToSubscribers(DataSource& source)
{
    // XXX maybe needs locks

    // Only send if there are subscribers
    if (!source.subscribers.empty())
    {
        QJsonObject data;
        QJsonArray  errs;

        getDataFromSource(data, errs, source);

        QString message = craftDataMessage(data, errs);

        if (!message.isEmpty())
        {
            for (auto sub : source.subscribers)
            {
                QMetaObject::invokeMethod(sub, [=] { sub->sendTextMessage(message); });
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
        rate["rate"]    = src.rate;

        rates.append(rate);
    }

    mdat["rates"] = rates;

    // ext settings
    if (m_settings)
    {
        static const QString entryTypeStrArr[] = {"int", "double", "bool", "string", "select"};

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
                    auto c    = entry.var.value<esi_inttype_t>();
                    s["min"]  = c.min;
                    s["max"]  = c.max;
                    s["step"] = c.step;
                    s["def"]  = c.def;
                    s["val"]  = c.val;
                    break;
                }

                case QUASAR_SETTING_ENTRY_DOUBLE:
                {
                    auto c    = entry.var.value<esi_doubletype_t>();
                    s["min"]  = c.min;
                    s["max"]  = c.max;
                    s["step"] = c.step;
                    s["def"]  = c.def;
                    s["val"]  = c.val;
                    break;
                }

                case QUASAR_SETTING_ENTRY_BOOL:
                {
                    auto c   = entry.var.value<esi_booltype_t>();
                    s["def"] = c.def;
                    s["val"] = c.val;
                    break;
                }

                case QUASAR_SETTING_ENTRY_STRING:
                {
                    auto c   = entry.var.value<esi_stringtype_t>();
                    s["def"] = c.def;
                    s["val"] = c.val;
                    break;
                }

                case QUASAR_SETTING_ENTRY_SELECTION:
                {
                    auto       c = entry.var.value<quasar_selection_options_t>();
                    QJsonArray vlist;

                    for (auto& e : c.list)
                    {
                        vlist.append(QJsonObject{{"name", e.name}, {"value", e.value}});
                    }

                    s["list"] = vlist;
                    s["val"]  = c.val;
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
        qWarning() << "Unknown data source" << source << "requested in extension" << m_name;
        return;
    }

    DataSource& data = m_datasources[source];

    // Set if changed
    if (data.enabled != enabled)
    {
        data.enabled = enabled;

        // Save to file
        QSettings settings;
        settings.setValue(getSettingsKey(source + QUASAR_DP_ENABLED), data.enabled);

        if (data.enabled && data.rate > QUASAR_POLLING_CLIENT && !data.subscribers.empty())
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
}

void DataExtension::setDataSourceRefresh(QString source, int64_t msec)
{
    if (!m_datasources.count(source))
    {
        qWarning() << "Unknown data source" << source << "requested in extension" << m_name;
        return;
    }

    DataSource& data = m_datasources[source];

    if (msec <= 0)
    {
        qWarning() << "Tried to set invalid refresh rate in data source" << source;
        return;
    }

    // Set if changed
    if (data.rate != msec)
    {
        data.rate = msec;

        // Save to file
        QSettings settings;
        settings.setValue(getSettingsKey(source + QUASAR_DP_RATE_PREFIX), (qlonglong) data.rate);

        // Refresh timer if exists
        if (nullptr != data.timer)
        {
            data.timer->setInterval(data.rate);
        }
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
            qWarning() << "Invalid setting key" << setkey;
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
                {
                    auto c = cset.var.value<esi_inttype_t>();
                    c.val  = setjs[setkey].toInt();
                    settings.setValue(getSettingsKey(name), setjs[setkey].toInt());
                    cset.var.setValue(c);
                    break;
                }
                case QUASAR_SETTING_ENTRY_DOUBLE:
                {
                    auto c = cset.var.value<esi_doubletype_t>();
                    c.val  = setjs[setkey].toDouble();
                    settings.setValue(getSettingsKey(name), setjs[setkey].toDouble());
                    cset.var.setValue(c);
                    break;
                }
                case QUASAR_SETTING_ENTRY_BOOL:
                {
                    auto c = cset.var.value<esi_booltype_t>();
                    c.val  = setjs[setkey].toBool();
                    settings.setValue(getSettingsKey(name), setjs[setkey].toBool());
                    cset.var.setValue(c);
                    break;
                }
                case QUASAR_SETTING_ENTRY_STRING:
                {
                    auto c = cset.var.value<esi_stringtype_t>();
                    c.val  = setjs[setkey].toString();
                    settings.setValue(getSettingsKey(name), setjs[setkey].toString());
                    cset.var.setValue(c);
                    break;
                }
                case QUASAR_SETTING_ENTRY_SELECTION:
                {
                    auto c = cset.var.value<quasar_selection_options_t>();
                    c.val  = setjs[setkey].toString();
                    settings.setValue(getSettingsKey(name), setjs[setkey].toString());
                    cset.var.setValue(c);
                    break;
                }
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
        qWarning() << "Unknown data source" << source << "requested in extension" << m_name;
        return;
    }

    DataSource& data = m_datasources[source];

    if (data.rate == QUASAR_POLLING_SIGNALED || data.rate == QUASAR_POLLING_CLIENT)
    {
        emit dataReady(source);
    }
}

void DataExtension::waitDataProcessed(QString source)
{
    if (!m_datasources.count(source))
    {
        qWarning() << "Unknown data source" << source << "requested in extension" << m_name;
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

void DataExtension::handleDataReadySignal(QString source)
{
    if (!m_datasources.count(source))
    {
        qWarning() << "Unknown data source" << source << "requested in extension" << m_name;
        return;
    }

    DataSource& dsrc = m_datasources[source];

    if (dsrc.rate == QUASAR_POLLING_CLIENT)
    {
        // pop poll queue
        QJsonObject data;
        QJsonArray  errs;
        auto        result = getDataFromSource(data, errs, dsrc);

        switch (result)
        {
            case GET_DATA_FAILED:
                qWarning().nospace() << "getDataFromSource(" << source << ") in extension " << m_name << " failed";
                break;
            case GET_DATA_DELAYED:
                qWarning().nospace() << "getDataFromSource(" << source << ") in extension " << m_name << " returned delayed data on signal ready";
                break;
            case GET_DATA_SUCCESS:
            {
                QString message = craftDataMessage(data, errs);

                if (!message.isEmpty())
                {
                    // XXX maybe needs locks
                    for (auto it = dsrc.pollqueue.begin(); it != dsrc.pollqueue.end();)
                    {
                        auto sub = *it;
                        QMetaObject::invokeMethod(sub, [=] { sub->sendTextMessage(message); });
                        it = dsrc.pollqueue.erase(it);
                    }
                }
                break;
            }
        }
    }
    else
    {
        // send to subscribers
        sendDataToSubscribers(dsrc);
    }
}

void DataExtension::createTimer(DataSource& data)
{
    if (data.enabled && !data.timer)
    {
        // Timer creation required
        data.timer = std::make_unique<QTimer>(this);
        connect(data.timer.get(), &QTimer::timeout, [this, &data] { sendDataToSubscribers(data); });

        data.timer->start(data.rate);
    }
}

QString DataExtension::craftDataMessage(const QJsonObject& data, const QJsonArray& errors)
{
    if (data.isEmpty() && errors.isEmpty())
    {
        // No data
        return QString();
    }

    QJsonObject msg;

    if (!data.isEmpty())
    {
        msg["data"] = QJsonObject{{m_name, data}};
    }

    if (!errors.isEmpty())
    {
        if (errors.count() == 1)
        {
            msg["errors"] = errors.at(0);
        }
        else
        {
            msg["errors"] = errors;
        }
    }

    QJsonDocument doc(msg);

    return QString::fromUtf8(doc.toJson());
}

DataExtension::DataSourceReturnState DataExtension::getDataFromSource(QJsonObject& data, QJsonArray& errs, DataSource& src, QString args)
{
    using namespace std::chrono;

    if (src.rate == QUASAR_POLLING_CLIENT && src.validtime)
    {
        // If this source is client polled and a validity duration is specified,
        // first check for expiry time and cached data

        if (src.expiry >= system_clock::now())
        {
            // If data hasn't expired yet, use the cached data
            data[src.name] = src.cacheddat;
            return GET_DATA_SUCCESS;
        }
    }

    if (!src.enabled)
    {
        // honour enabled flag
        qWarning() << "Data source" << src.name << "is disabled";
        return GET_DATA_FAILED;
    }

    quasar_return_data_t rett;

    // Init to undefined
    rett.val = QJsonValue(QJsonValue::Undefined);

    QByteArray chargs;

    if (!args.isEmpty())
    {
        chargs = args.toUtf8();
    }

    // Poll extension for data source
    if (!m_extension->get_data(src.uid, &rett, chargs.isEmpty() ? nullptr : chargs.data()))
    {
        qWarning().nospace() << "get_data(" << m_name << ", " << src.name << ") failed";
        return GET_DATA_FAILED;
    }

    if (rett.val.isNull())
    {
        // Data is purposely set to a null return
        return GET_DATA_SUCCESS;
    }

    if (rett.val.isUndefined())
    {
        if (src.rate == QUASAR_POLLING_CLIENT)
        {
            // Allow empty return (for async data)
            return GET_DATA_DELAYED;
        }

        // Disallow any other source types from setting no data
        return GET_DATA_FAILED;
    }

    // If we have valid data here:
    if (src.rate == QUASAR_POLLING_CLIENT && src.validtime)
    {
        // If validity time duration is set, cache the data
        src.cacheddat = rett.val;
        src.expiry    = system_clock::now() + milliseconds(src.validtime);
    }

    data[src.name] = rett.val;

    return GET_DATA_SUCCESS;
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
                QMetaObject::invokeMethod(sub, [=] { sub->sendTextMessage(payload); });
            }
        }
    }
}
