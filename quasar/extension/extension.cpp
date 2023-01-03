#include "extension.h"

#include "extension_support_internal.h"

#include "common/config.h"
#include "server/server.h"

#include <QLibrary>

#include <spdlog/spdlog.h>

#define CHAR_TO_UTF8(d, x) \
  x[sizeof(x) - 1] = 0;    \
  d                = std::string(x);

size_t Extension::_uid = 0;

Extension::Extension(quasar_ext_info_t* info,
    extension_destroy                   destroyfunc,
    const std::string&                  path,
    std::shared_ptr<Server>             srv,
    std::shared_ptr<Config>             cfg) :
    extensionInfo{info},
    destroyFunc{destroyfunc},
    libpath{path},
    initialized{false},
    server{srv},
    config{cfg}
{
    if (nullptr == extensionInfo)
    {
        throw std::invalid_argument("null extensionInfo");
    }

    // Currently only support latest API version
    if (extensionInfo->api_version != QUASAR_API_VERSION)
    {
        throw std::invalid_argument("unsupported API version");
    }

    if (nullptr == extensionInfo->fields)
    {
        throw std::invalid_argument("null extension fields struct");
    }

    CHAR_TO_UTF8(name, extensionInfo->fields->name);
    CHAR_TO_UTF8(fullname, extensionInfo->fields->fullname);
    CHAR_TO_UTF8(author, extensionInfo->fields->author);
    CHAR_TO_UTF8(description, extensionInfo->fields->description);
    CHAR_TO_UTF8(version, extensionInfo->fields->version);
    CHAR_TO_UTF8(url, extensionInfo->fields->url);

    if (name.empty() or fullname.empty())
    {
        throw std::runtime_error("Invalid extension identifier or name");
    }

    // register data sources
    if (nullptr != extensionInfo->dataSources)
    {
        for (size_t i = 0; i < extensionInfo->numDataSources; i++)
        {
            CHAR_TO_UTF8(std::string srcname, extensionInfo->dataSources[i].name);

            auto topic = fmt::format("{}/{}", name, srcname);

            if (datasources.count(topic))
            {
                SPDLOG_WARN("Extension {} tried to register more than one topic '{}'", name, topic);
                continue;
            }

            DataSource& source = datasources[topic];

            // TODO rewrite this garbage

            source.settings.enabled = true;
            source.settings.rate    = extensionInfo->dataSources[i].rate;

            source.topic            = topic;
            source.validtime        = extensionInfo->dataSources[i].validtime;
            source.uid = extensionInfo->dataSources[i].uid = ++Extension::_uid;

            config.lock()->AddDataSourceSetting(topic, &source.settings);

            // Initialize type specific fields
            if (source.settings.rate == QUASAR_POLLING_SIGNALED)
            {
                source.locks = std::make_unique<DataLock>();
            }

            if (source.settings.rate == QUASAR_POLLING_CLIENT)
            {
                source.cache.expiry = std::chrono::system_clock::now();
            }

            SPDLOG_INFO("Extension {} registering topic '{}'", name, topic);
        }
    }

    // create settings
    if (extensionInfo->create_settings)
    {
        // TODO extension settings
        // m_settings.reset(m_extension->create_settings());

        // if (!m_settings)
        // {
        //     throw std::runtime_error("extension create_settings() failed");
        // }

        // // Fill saved settings if any
        // for (auto it = m_settings->map.begin(); it != m_settings->map.end(); ++it)
        // {
        //     auto& def = it.value();

        //     switch (def.type)
        //     {
        //         case QUASAR_SETTING_ENTRY_INT:
        //             {
        //                 auto c = def.var.value<esi_inttype_t>();
        //                 c.val  = settings.value(getSettingsKey(it.key()), c.def).toInt();
        //                 def.var.setValue(c);
        //                 break;
        //             }

        //         case QUASAR_SETTING_ENTRY_DOUBLE:
        //             {
        //                 auto c = def.var.value<esi_doubletype_t>();
        //                 c.val  = settings.value(getSettingsKey(it.key()), c.def).toDouble();
        //                 def.var.setValue(c);
        //                 break;
        //             }

        //         case QUASAR_SETTING_ENTRY_BOOL:
        //             {
        //                 auto c = def.var.value<esi_doubletype_t>();
        //                 c.val  = settings.value(getSettingsKey(it.key()), c.def).toBool();
        //                 def.var.setValue(c);
        //                 break;
        //             }

        //         case QUASAR_SETTING_ENTRY_STRING:
        //             {
        //                 auto c = def.var.value<esi_stringtype_t>();
        //                 c.val  = settings.value(getSettingsKey(it.key()), c.def).toString();
        //                 def.var.setValue(c);
        //                 break;
        //             }

        //         case QUASAR_SETTING_ENTRY_SELECTION:
        //             {
        //                 auto c = def.var.value<quasar_selection_options_t>();
        //                 c.val  = settings.value(getSettingsKey(it.key()), c.list.at(0).value).toString();
        //                 def.var.setValue(c);
        //                 break;
        //             }
        //     }
        // }

        // updateExtensionSettings();
    }
}

bool Extension::TopicExists(const std::string& topic) const
{
    return (datasources.count(topic) > 0);
}

bool Extension::TopicAcceptsSubscribers(const std::string& topic)
{
    if (!TopicExists(topic))
    {
        return false;
    }

    DataSource&                         dsrc = datasources[topic];

    std::shared_lock<std::shared_mutex> lk(dsrc.mutex);

    if (dsrc.settings.rate == QUASAR_POLLING_CLIENT)
    {
        return false;
    }

    return true;
}

bool Extension::AddSubscriber(void* subscriber, const std::string& topic, int count)
{
    if (!subscriber)
    {
        SPDLOG_CRITICAL("Unknown subscriber.");
        return false;
    }

    if (!TopicExists(topic))
    {
        SPDLOG_WARN("Unknown topic {} requested in extension {}", topic, name);
        return false;
    }

    DataSource&                        dsrc = datasources[topic];

    std::lock_guard<std::shared_mutex> lk(dsrc.mutex);

    if (dsrc.settings.rate == QUASAR_POLLING_CLIENT)
    {
        SPDLOG_WARN("Topic '{}' in extension {} requested by widget does not accept subscribers", topic, name);
        return false;
    }

    dsrc.subscribers = count;

    if (dsrc.settings.rate > QUASAR_POLLING_CLIENT)
    {
        createTimer(dsrc);
    }

    // TODO: Send settings if applicable
    // auto payload = craftSettingsMessage();

    // if (!payload.isEmpty())
    // {
    //     QMetaObject::invokeMethod(subscriber, [=] { subscriber->sendTextMessage(payload); });
    // }

    return true;
}

void Extension::RemoveSubscriber(void* subscriber, const std::string& topic, int count)
{
    if (!subscriber)
    {
        SPDLOG_WARN("Null subscriber.");
        return;
    }

    if (!datasources.count(topic))
    {
        SPDLOG_WARN("Unknown topic {} requested in extension {}", topic, name);
        return;
    }

    DataSource&                        dsrc = datasources[topic];

    std::lock_guard<std::shared_mutex> lk(dsrc.mutex);

    SPDLOG_INFO("Widget unsubscribed from topic {}", dsrc.topic);

    dsrc.subscribers = count;

    // Stop timer if no subscribers
    if (dsrc.subscribers <= 0)
    {
        dsrc.timer.reset();
    }
}

Extension::DataSourceReturnState Extension::getDataFromSource(jsoncons::json& msg, DataSource& src, std::string args)
{
    using namespace std::chrono;

    jsoncons::json& j = msg[src.topic];

    if (!src.settings.enabled)
    {
        // honour enabled flag
        SPDLOG_WARN("Topic {} is disabled", src.topic);
        return GET_DATA_FAILED;
    }

    if (src.settings.rate == QUASAR_POLLING_CLIENT && src.validtime)
    {
        // If this source is client polled and a validity duration is specified,
        // first check for expiry time and cached data

        if (src.cache.expiry >= system_clock::now())
        {
            // If data hasn't expired yet, use the cached data
            j = src.cache.data;
            return GET_DATA_SUCCESS;
        }
    }

    quasar_return_data_t rett;

    // Poll extension for data source
    if (!extensionInfo->get_data(src.uid, &rett, args.empty() ? nullptr : args.data()))
    {
        if (!rett.errors.empty())
        {
            msg["errors"].insert(msg["errors"].end_elements(), rett.errors.begin(), rett.errors.end());
        }

        SPDLOG_WARN("get_data({}, {}) failed", name, src.topic);
        return GET_DATA_FAILED;
    }

    if (!rett.errors.empty())
    {
        msg["errors"].insert(msg["errors"].end_elements(), rett.errors.begin(), rett.errors.end());
    }

    if (not rett.val)
    {
        if (src.settings.rate == QUASAR_POLLING_CLIENT)
        {
            // Allow empty return (for async data)
            return GET_DATA_DELAYED;
        }

        // Disallow any other source types from setting no data
        return GET_DATA_FAILED;
    }

    if (rett.val.value().is_null())
    {
        // Data is purposely set to a null return
        return GET_DATA_SUCCESS;
    }

    // If we have valid data here:
    if (src.settings.rate == QUASAR_POLLING_CLIENT && src.validtime)
    {
        // If validity time duration is set, cache the data
        src.cache.data   = rett.val.value();
        src.cache.expiry = system_clock::now() + milliseconds(src.validtime);
    }

    j = rett.val.value();

    return GET_DATA_SUCCESS;
}

void Extension::sendDataToSubscribers(DataSource& src)
{
    std::lock_guard<std::shared_mutex> lk(src.mutex);

    // Only send if there are subscribers
    if (src.subscribers > 0)
    {
        jsoncons::json j{
            jsoncons::json_object_arg,
            {{src.topic, jsoncons::json{jsoncons::json_object_arg}}, {"errors", jsoncons::json{jsoncons::json_array_arg}}}
        };

        getDataFromSource(j, src);

        std::string message{};

        j.dump(message);

        if (!message.empty())
        {
            server.lock()->PublishData(src.topic, message);
        }
    }

    // Signal data processed
    if (nullptr != src.locks)
    {
        {
            std::lock_guard<std::mutex> lk(src.locks->mutex);
            src.locks->processed = true;
        }

        src.locks->cv.notify_one();
    }
}

void Extension::createTimer(DataSource& src)
{
    if (src.settings.enabled && !src.timer)
    {
        // Timer creation required
        src.timer = std::make_unique<Timer>();

        src.timer->setInterval(
            [this, &src] {
                sendDataToSubscribers(src);
            },
            src.settings.rate);
    }
}

Extension::~Extension()
{
    if (nullptr != extensionInfo->shutdown)
    {
        extensionInfo->shutdown(this);
    }

    // Do some explicit cleanup
    for (auto& [name, src] : datasources)
    {
        config.lock()->WriteDataSourceSetting(name, &src.settings);
        src.timer.reset();
        src.locks.reset();
    }

    // extension is responsible for cleanup of quasar_ext_info_t*
    destroyFunc(extensionInfo);
    extensionInfo = nullptr;
}

Extension* Extension::Load(const std::string& libpath, std::shared_ptr<Config> cfg, std::shared_ptr<Server> srv)
{
    QLibrary lib(QString::fromStdString(libpath));

    if (!lib.load())
    {
        SPDLOG_WARN(lib.errorString().toStdString());
        return nullptr;
    }

    extension_load    loadfunc    = (extension_load) lib.resolve("quasar_ext_load");
    extension_destroy destroyfunc = (extension_destroy) lib.resolve("quasar_ext_destroy");

    if (!loadfunc or !destroyfunc)
    {
        SPDLOG_WARN("Failed to resolve extension API in {}", libpath);
        return nullptr;
    }

    quasar_ext_info_t* p = loadfunc();

    if (!p or !p->init or !p->shutdown or !p->get_data or !p->fields or !p->dataSources)
    {
        SPDLOG_WARN("quasar_ext_load failed in {}: required extension data missing", libpath);
        return nullptr;
    }

    try
    {
        Extension* extension = new Extension(p, destroyfunc, libpath, srv, cfg);
        return extension;
    } catch (std::exception e)
    {
        SPDLOG_WARN("Exception: {} while allocating {}", e.what(), libpath);
    }

    return nullptr;
}

void Extension::Initialize()
{
    if (!initialized)
    {
        // initialize the extension
        if (!extensionInfo->init(this))
        {
            throw std::runtime_error("extension init() failed");
        }

        // TODO extension settings
        // if (m_settings)
        // {
        //     updateExtensionSettings();
        // }

        initialized = true;

        SPDLOG_INFO("Extension {} initialized.", GetName());
    }
}

void Extension::PollDataForSending(jsoncons::json& json, const std::vector<std::string>& topics, const std::string& args, void* client)
{
    for (auto& topic : topics)
    {
        if (!datasources.count(topic))
        {
            auto m = "Unknown topic " + topic + " requested in extension " + name;  // + " by widget " + widgetName;
            json["errors"].push_back(m);

            SPDLOG_WARN(m);
            continue;
        }

        DataSource&                        dsrc = datasources[topic];

        std::lock_guard<std::shared_mutex> lk(dsrc.mutex);

        json[dsrc.topic] = jsoncons::json{jsoncons::json_object_arg};

        auto result      = getDataFromSource(json, dsrc, args);

        switch (result)
        {
            case GET_DATA_FAILED:
                {
                    auto m = "getDataFromSource(" + topic + ") failed in extension " + name;  // + " requested by widget " + widgetName;
                    json["errors"].push_back(m);
                    SPDLOG_WARN(m);
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
}
