#include "extension.h"

#include "extension_support_internal.h"

#include <QLibrary>

#include <spdlog/spdlog.h>

#define CHAR_TO_UTF8(d, x) \
  x[sizeof(x) - 1] = 0;    \
  d                = std::string(x);

uintmax_t Extension::_uid = 0;

Extension::Extension(quasar_ext_info_t* p, extension_destroy destroyfunc, const std::string& path, std::shared_ptr<Server> srv, std::shared_ptr<Config> cfg) :
    extensionInfo{p},
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

            if (datasources.count(srcname))
            {
                SPDLOG_WARN("Extension {} tried to register more than one data source {}", name, srcname);
                continue;
            }

            SPDLOG_INFO("Extension {} registering data source {}", name, srcname);

            DataSource& source = datasources[srcname];

            // TODO get extension settings

            // TODO rewrite this garbage

            source.enabled   = true;
            source.name      = srcname;
            source.rate      = extensionInfo->dataSources[i].rate;
            source.validtime = extensionInfo->dataSources[i].validtime;
            source.uid = extensionInfo->dataSources[i].uid = ++Extension::_uid;

            // Initialize type specific fields
            if (source.rate == QUASAR_POLLING_SIGNALED)
            {
                source.locks = std::make_unique<DataLock>();
            }

            if (source.rate == QUASAR_POLLING_CLIENT)
            {
                source.expiry = std::chrono::system_clock::now();
            }
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

Extension::DataSourceReturnState Extension::getDataFromSource(jsoncons::json& msg, DataSource& src, std::string args)
{
    using namespace std::chrono;

    jsoncons::json& j = msg["data"][GetName()];

    if (src.rate == QUASAR_POLLING_CLIENT && src.validtime)
    {
        // If this source is client polled and a validity duration is specified,
        // first check for expiry time and cached data

        if (src.expiry >= system_clock::now())
        {
            // If data hasn't expired yet, use the cached data
            j[src.name] = src.cacheddat;
            return GET_DATA_SUCCESS;
        }
    }

    if (!src.enabled)
    {
        // honour enabled flag
        SPDLOG_WARN("Data source {} is disabled", src.name);
        return GET_DATA_FAILED;
    }

    quasar_return_data_t rett;

    // Poll extension for data source
    if (!extensionInfo->get_data(src.uid, &rett, args.empty() ? nullptr : args.data()))
    {
        if (!rett.errors.empty())
        {
            msg["errors"].insert(msg["errors"].end_elements(), rett.errors.begin(), rett.errors.end());
        }

        SPDLOG_WARN("get_data({}, {}) failed", name, src.name);
        return GET_DATA_FAILED;
    }

    if (!rett.errors.empty())
    {
        msg["errors"].insert(msg["errors"].end_elements(), rett.errors.begin(), rett.errors.end());
    }

    if (not rett.val)
    {
        if (src.rate == QUASAR_POLLING_CLIENT)
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
    if (src.rate == QUASAR_POLLING_CLIENT && src.validtime)
    {
        // If validity time duration is set, cache the data
        src.cacheddat = rett.val.value();
        src.expiry    = system_clock::now() + milliseconds(src.validtime);
    }

    j[src.name] = rett.val.value();

    return GET_DATA_SUCCESS;
}

Extension::~Extension()
{
    if (nullptr != extensionInfo->shutdown)
    {
        extensionInfo->shutdown(this);
    }

    // Do some explicit cleanup
    for (auto it = datasources.begin(); it != datasources.end(); ++it)
    {
        // TODO timers
        // it.value().timer.reset();

        it->second.locks.reset();
        it->second.subscribers.clear();
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

std::string Extension::PollDataForSending(const std::vector<std::string>& sources, const std::string& args, void* client)
{
    jsoncons::json j;
    j["data"]   = jsoncons::json(jsoncons::json_object_arg,
          {
            {GetName(), {}}
    });
    j["errors"] = jsoncons::json(jsoncons::json_array_arg);

    std::string message{};

    for (auto& src : sources)
    {
        if (!datasources.count(src))
        {
            auto m = "Unknown data source " + src + " requested in extension " + name;  // + " by widget " + widgetName;
            j["errors"].push_back(m);

            SPDLOG_WARN(m);
            continue;
        }

        DataSource&                        dsrc = datasources[src];

        std::lock_guard<std::shared_mutex> lk(dsrc.mutex);

        auto                               result = getDataFromSource(j, dsrc, args);

        switch (result)
        {
            case GET_DATA_FAILED:
                {
                    auto m = "getDataFromSource(" + src + ") failed in extension " + name;  // + " requested by widget " + widgetName;
                    j["errors"].push_back(m);
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

    jsoncons::encode_json(j, message);

    return message;
}
