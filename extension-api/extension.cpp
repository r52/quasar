#include "extension.h"

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

    if (name.empty() || fullname.empty())
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

            SPDLOG_INFO("Extension {} registering data source", name, srcname);

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

Extension::~Extension() {}

Extension* Extension::load(const std::string& libpath, std::shared_ptr<Config> cfg, std::shared_ptr<Server> srv)
{
    QLibrary lib(QString::fromStdString(libpath));

    if (!lib.load())
    {
        SPDLOG_WARN(lib.errorString().toStdString());
        return nullptr;
    }

    extension_load    loadfunc    = (extension_load) lib.resolve("quasar_ext_load");
    extension_destroy destroyfunc = (extension_destroy) lib.resolve("quasar_ext_destroy");

    if (!loadfunc || !destroyfunc)
    {
        SPDLOG_WARN("Failed to resolve extension API in {}", libpath);
        return nullptr;
    }

    quasar_ext_info_t* p = loadfunc();

    if (!p || !p->init || !p->shutdown || !p->get_data || !p->fields || !p->dataSources)
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

void Extension::initialize()
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
