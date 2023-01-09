#include "applauncher.h"

#include <string>

#include "api/extension_support.hpp"
#include "common/settings.h"
#include "extension/extension.h"

#include <QDesktopServices>
#include <QFileInfo>
#include <QProcess>
#include <QString>
#include <QUrl>

#include <jsoncons/json.hpp>
#include <spdlog/spdlog.h>

struct AppLauncherData
{
    std::string command;
    std::string file;
    std::string start;
    std::string args;
    std::string icon;
};

JSONCONS_ALL_MEMBER_TRAITS(AppLauncherData, command, file, start, args, icon);

namespace
{
    std::vector<AppLauncherData> applist;
}  // namespace

quasar_data_source_t sources[] = {
    {  "list", QUASAR_POLLING_CLIENT, 0, 0},
    {"launch", QUASAR_POLLING_CLIENT, 0, 0}
};

bool applauncher_init(quasar_ext_handle handle)
{
    return true;
}

bool applauncher_shutdown(quasar_ext_handle handle)
{
    return true;
}

bool applauncher_get_data(size_t srcUid, quasar_data_handle hData, char* args)
{
    if (srcUid != sources[0].uid && srcUid != sources[1].uid)
    {
        SPDLOG_WARN("Unknown source {}", srcUid);
        return false;
    }

    if (srcUid == sources[0].uid)
    {
        // list
        if (applist.empty())
        {
            auto m = "Empty applist";
            SPDLOG_WARN(m);
            quasar_append_error(hData, m);
            return true;
        }

        jsoncons::json sendlist{jsoncons::json_array_arg};
        for (const auto& item : applist)
        {
            sendlist.push_back(jsoncons::json{
                jsoncons::json_object_arg,
                {{"command", item.command}, {"icon", item.icon}}
            });
        }

        std::string res{};
        sendlist.dump(res);

        quasar_set_data_json_hpp(hData, res);
    }
    else if (srcUid == sources[1].uid)
    {
        // launch
        if (!args)
        {
            auto m = "No arguments provided for 'applauncher/launch'";
            SPDLOG_WARN(m);
            quasar_append_error(hData, m);
            return false;
        }

        auto it = std::find_if(applist.begin(), applist.end(), [&](const auto& item) {
            return item.command == args;
        });

        if (it == applist.end())
        {
            auto m = fmt::format("Unknown launcher command '{}'", args);
            SPDLOG_WARN(m);
            quasar_append_error(hData, m.c_str());
            return false;
        }

        auto&   d   = *it;

        QString cmd = QString::fromStdString(d.file);

        if (cmd.contains("://"))
        {
            // treat as url
            SPDLOG_INFO("Launching URL {}", cmd.toStdString());
            QDesktopServices::openUrl(QUrl(cmd));
        }
        else
        {
            SPDLOG_INFO("Launching {} {}", cmd.toStdString(), d.args);

            // treat as file/command
            QFileInfo info(cmd);

            if (info.exists())
            {
                cmd = info.canonicalFilePath();
            }

            bool result = QProcess::startDetached(cmd, QStringList() << QString::fromStdString(d.args), QString::fromStdString(d.start));

            if (!result)
            {
                SPDLOG_WARN("Failed to launch {}", cmd.toStdString());
            }
        }
    }

    return true;
}

quasar_settings_t* applauncher_create_settings(quasar_ext_handle handle)
{
    quasar_settings_t* settings = quasar_create_settings(handle);

    // Just a dummy setting
    quasar_add_bool_setting(handle, settings, "enabled", "Enabled", true);

    return settings;
}

void applauncher_update_settings(quasar_settings_t* settings)
{
    applist = jsoncons::decode_json<std::vector<AppLauncherData>>(Settings::internal.applauncher.GetValue());
}

quasar_ext_info_fields_t fields = {"applauncher", "App Launcher", "3.0", "r52", "App Launcher internal extension for Quasar", "https://github.com/r52/quasar"};

quasar_ext_info_t        info   = {
    QUASAR_API_VERSION,
    &fields,

    std::size(sources),
    sources,

    applauncher_init,             // init
    applauncher_shutdown,         // shutdown
    applauncher_get_data,         // data
    applauncher_create_settings,  // create setting
    applauncher_update_settings   // update setting
};

quasar_ext_info_t* applauncher_load(void)
{
    return &info;
}

void applauncher_destroy(quasar_ext_info_t* info) {}
