#include "extension.h"

#include "extension_support_internal.h"

#include "common/config.h"
#include "server/server.h"

#include <QLibrary>

#include <spdlog/spdlog.h>

#define CHAR_TO_UTF8(d, x) \
  x[sizeof(x) - 1] = 0;    \
  d                = std::string{x};

#define EXTKEY(key) fmt::format("{}/{}", GetName(), key)

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

    auto cfl = config.lock();

    // register data sources
    if (nullptr != extensionInfo->dataSources)
    {
        for (size_t i = 0; i < extensionInfo->numDataSources; i++)
        {
            CHAR_TO_UTF8(const std::string srcname, extensionInfo->dataSources[i].name);

            const auto topic = fmt::format("{}/{}", name, srcname);

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

            cfl->AddDataSourceSetting(topic, &source.settings);

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
        // extension settings
        auto result = extensionInfo->create_settings(this);

        if (!result)
        {
            throw std::runtime_error("extension create_settings() failed");
        }

        // Fill saved settings if any
        for (auto& def : settings)
        {
            std::visit(
                [&](auto&& arg) {
                    cfl->ReadSetting(arg);
                },
                def);
        }

        updateExtensionSettings();
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

    DataSource& dsrc = datasources[topic];

    if (dsrc.settings.rate == QUASAR_POLLING_CLIENT)
    {
        SPDLOG_WARN("Topic '{}' in extension {} requested by widget does not accept subscribers", topic, name);
        return false;
    }

    {
        std::lock_guard<std::shared_mutex> lk(dsrc.mutex);

        dsrc.subscribers = count;

        if (dsrc.settings.rate > QUASAR_POLLING_CLIENT)
        {
            createTimer(dsrc);
        }
    }

    // Send settings if applicable
    auto payload = craftSettingsMessage();

    if (!payload.empty())
    {
        // Send the payload
        server.lock()->SendDataToClient((PerSocketData*) subscriber, payload);
    }

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

void Extension::GetMetadataJSON(jsoncons::json& json, bool settings_only)
{
    static const auto metakey = EXTKEY("metadata");
    static const auto setkey  = EXTKEY("settings");

    jsoncons::json&   mdat    = json[metakey];
    jsoncons::json&   set     = json[setkey];

    if (!settings_only)
    {
        // ext info
        mdat["name"]        = name;
        mdat["fullname"]    = fullname;
        mdat["version"]     = version;
        mdat["author"]      = author;
        mdat["description"] = description;
        mdat["url"]         = url;
    }

    mdat["rates"] = jsoncons::json{jsoncons::json_array_arg};

    // ext rates

    for (auto& [key, src] : datasources)
    {
        std::shared_lock<std::shared_mutex> lk(src.mutex);

        jsoncons::json                      source(jsoncons::json_object_arg,
                                 {
                {   "name",            src.topic},
                {"enabled", src.settings.enabled},
                {   "rate",    src.settings.rate}
        });

        mdat["rates"].push_back(source);
    }

    // ext settings
    if (!settings.empty())
    {
        set = jsoncons::json{jsoncons::json_array_arg};

        for (auto& def : settings)
        {
            jsoncons::json setting(jsoncons::json_object_arg);

            std::visit(
                [&](auto&& arg) {
                    using T         = std::decay_t<decltype(arg)>;

                    setting["name"] = arg.GetLabel();
                    setting["desc"] = arg.GetDescription();

                    if constexpr (std::is_same_v<T, Settings::Setting<int>>)
                    {
                        setting["type"]       = "int";

                        auto [min, max, step] = arg.GetMinMaxStep();

                        setting["min"]        = min;
                        setting["max"]        = max;
                        setting["step"]       = step;
                        setting["def"]        = arg.GetDefault();
                        setting["val"]        = arg.GetValue();
                    }
                    else if constexpr (std::is_same_v<T, Settings::Setting<double>>)
                    {
                        setting["type"]       = "double";

                        auto [min, max, step] = arg.GetMinMaxStep();

                        setting["min"]        = min;
                        setting["max"]        = max;
                        setting["step"]       = step;
                        setting["def"]        = arg.GetDefault();
                        setting["val"]        = arg.GetValue();
                    }
                    else if constexpr (std::is_same_v<T, Settings::Setting<bool>>)
                    {
                        setting["type"] = "bool";
                        setting["def"]  = arg.GetDefault();
                        setting["val"]  = arg.GetValue();
                    }
                    else if constexpr (std::is_same_v<T, Settings::Setting<std::string>>)
                    {
                        setting["type"]     = "string";
                        setting["def"]      = arg.GetDefault();
                        setting["val"]      = arg.GetValue();
                        setting["password"] = arg.GetIsPassword();
                    }
                    else if constexpr (std::is_same_v<T, Settings::SelectionSetting<std::string>>)
                    {
                        setting["type"] = "select";
                        setting["list"] = jsoncons::json{jsoncons::json_array_arg};

                        auto& c         = arg.GetOptions();

                        for (auto& [value, name] : c)
                        {
                            jsoncons::json opt{
                                jsoncons::json_object_arg,
                                {{"name", name}, {"value", value}}
                            };

                            setting["list"].push_back(opt);
                        }

                        setting["val"] = arg.GetValue();
                    }
                    else
                    {
                        SPDLOG_WARN("Non-exhaustive visitor!");
                    }
                },
                def);

            set.push_back(setting);
        }
    }
    else
    {
        set = jsoncons::json::null();
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

    j = std::move(rett.val.value());

    return GET_DATA_SUCCESS;
}

void Extension::sendDataToSubscribers(DataSource& src)
{
    std::lock_guard<std::shared_mutex> lk(src.mutex);

    // Only send if there are subscribers
    if (src.subscribers > 0)
    {
        src.buffer.clear();

        jsoncons::json j{
            jsoncons::json_object_arg,
            {{src.topic, jsoncons::json{jsoncons::json_object_arg}}, {"errors", jsoncons::json{jsoncons::json_array_arg}}}
        };

        getDataFromSource(j, src);

        j.dump(src.buffer);

        if (!src.buffer.empty())
        {
            server.lock()->PublishData(src.topic, src.buffer);
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

void Extension::updateExtensionSettings()
{
    if (!settings.empty() && extensionInfo->update)
    {
        extensionInfo->update((quasar_settings_t*) &settings);

        propagateSettingsToSubscribers();
    }
}

void Extension::propagateSettingsToSubscribers()
{
    if (!settings.empty())
    {
        auto payload = craftSettingsMessage();

        if (!payload.empty())
        {
            // Send the payload
            for (auto& [name, source] : datasources)
            {
                std::shared_lock<std::shared_mutex> lk(source.mutex);

                server.lock()->PublishData(source.topic, payload);
            }
        }
    }
}

const std::string Extension::craftSettingsMessage()
{
    std::string payload{};

    if (!settings.empty())
    {
        static const auto metakey = EXTKEY("metadata");
        static const auto setkey  = EXTKEY("settings");

        jsoncons::json    j{
            jsoncons::json_object_arg,
               {{metakey, jsoncons::json_object_arg}, {setkey, jsoncons::json_object_arg}}
        };

        GetMetadataJSON(j, true);

        j.dump(payload);
    }

    return payload;
}

Extension::~Extension()
{
    auto cfl = config.lock();

    // Do some explicit cleanup
    for (auto& [name, src] : datasources)
    {
        src.timer.reset();
        src.locks.reset();
        cfl->WriteDataSourceSetting(name, &src.settings);
    }

    // Save extension settings
    for (auto& def : settings)
    {
        std::visit(
            [&](auto&& arg) {
                cfl->WriteSetting(arg);
            },
            def);
    }

    if (nullptr != extensionInfo->shutdown)
    {
        extensionInfo->shutdown(this);
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

        if (!settings.empty())
        {
            updateExtensionSettings();
        }

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
            auto m = fmt::format("Unknown topic {} requested in extension {}", topic, name);  // + " by widget " + widgetName;
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
                    auto m = fmt::format("getDataFromSource({}) failed in extension {}", topic, name);  // + " requested by widget " + widgetName;
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