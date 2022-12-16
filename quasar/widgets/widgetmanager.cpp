#include "widgetmanager.h"

#include "../server/server.h"
#include "quasarwidget.h"

#include "settings.h"

#include <algorithm>
#include <fstream>
#include <ranges>
#include <regex>

#include <QMessageBox>
#include <QObject>

#include <daw/json/daw_json_link.h>
#include <spdlog/spdlog.h>

WidgetManager::WidgetManager(std::shared_ptr<Server> serv) : server{serv} {}

WidgetManager::~WidgetManager()
{
    widgetMap.clear();
}

bool WidgetManager::LoadWidget(const std::string& filename, std::shared_ptr<Config> config, bool userAction)
{
    if (filename.empty())
    {
        SPDLOG_ERROR("Error loading widget: Null filename");
        return false;
    }

    auto def_file = std::fstream(filename);

    if (!def_file)
    {
        SPDLOG_ERROR("Failed to load {}", filename);
        return false;
    }

    std::string json_doc;
    std::copy(std::istreambuf_iterator<char>(def_file), std::istreambuf_iterator<char>(), std::back_inserter(json_doc));

    WidgetDefinition def;

    try
    {
        def = parse_definition(json_doc);

    } catch (daw::json::json_exception const& je)
    {
        SPDLOG_ERROR("Error parsing widget definition file '{}': {}", filename, to_formatted_string(je));
        return false;
    }

    def.fullpath = filename;

    // Verify extension dependencies
    if (def.required)
    {
        bool        pass    = true;
        std::string failext = "";

        auto        serv    = server.lock();

        for (auto p : def.required.value())
        {
            if (!serv->FindExtension(p))
            {
                pass    = false;
                failext = p;
                break;
            }
        }

        if (!pass)
        {
            SPDLOG_ERROR("Missing extension \"{}\" for widget \"{}\"", failext, filename);

            QMessageBox::warning(nullptr,
                                 QObject::tr("Missing Extension"),
                                 QObject::tr("Extension \"%1\" is required for widget \"%2\". Please install this extension and try again.")
                                     .arg(QString::fromStdString(failext), QString::fromStdString(filename)),
                                 QMessageBox::Ok);

            return false;
        }
    }

    if (userAction && !acceptSecurityWarnings(def))
    {
        SPDLOG_WARN("Denied loading widget {}", filename);
        return false;
    }

    // Generate unique widget name
    std::string                         widgetName = def.name;
    int                                 idx        = 2;

    std::unique_lock<std::shared_mutex> lk(mutex);

    while (widgetMap.count(widgetName) > 0)
    {
        widgetName = def.name + std::to_string(idx++);
    }

    SPDLOG_INFO("Loading widget \"{}\" ({})", widgetName, def.fullpath);

    auto widget = std::make_unique<QuasarWidget>(widgetName, def, server.lock(), shared_from_this(), config);

    widget->show();

    if (userAction)
    {
        // Add to loaded
        auto loaded = getLoadedWidgetsList();
        loaded.push_back(widget->GetFullPath());
        saveLoadedWidgetsList(loaded);
    }

    widgetMap.insert(std::make_pair(widgetName, std::move(widget)));

    return true;
}

void WidgetManager::CloseWidget(QuasarWidget* widget)
{
    const auto name = widget->GetName();

    SPDLOG_INFO("Closing widget \"{}\" ({})", widget->GetName(), widget->GetFullPath());

    {
        std::unique_lock<std::shared_mutex> lk(mutex);

        // Remove from registry
        auto it = widgetMap.find(name);

        if (it != widgetMap.end())
        {
            Q_ASSERT((it->second.get()) == widget);
            // Release unique_ptr ownership to allow Qt gc to kick in
            it->second.release();
            widgetMap.erase(it);
        }

        widget->deleteLater();
    }

    // Remove from loaded
    auto loaded = getLoadedWidgetsList();

    auto result = std::find(loaded.begin(), loaded.end(), widget->GetFullPath());
    if (result != loaded.end())
    {
        loaded.erase(result);
    }

    saveLoadedWidgetsList(loaded);
}

void WidgetManager::LoadStartupWidgets(std::shared_ptr<Config> config)
{
    auto loaded = getLoadedWidgetsList();

    for (auto file : loaded)
    {
        LoadWidget(file, config, false);
    }
}

std::vector<QuasarWidget*> WidgetManager::GetWidgets()
{
    std::vector<QuasarWidget*> widgets;

    {
        std::unique_lock<std::shared_mutex> lk(mutex);

        std::transform(widgetMap.begin(), widgetMap.end(), std::back_inserter(widgets), [](auto& pair) {
            return pair.second.get();
        });
    }

    return widgets;
}

bool WidgetManager::acceptSecurityWarnings(const WidgetDefinition& def)
{
    if (!def.remoteAccess.value_or(false))
    {
        return true;
    }

    bool accept = false;

    auto reply  = QMessageBox::warning(nullptr,
                                      QObject::tr("Remote Access"),
                                      QObject::tr("This widget requires remote access to external URLs. This may pose a security risk.\n\nContinue loading?"),
                                      QMessageBox::Ok | QMessageBox::Cancel);

    return (reply == QMessageBox::Ok);
}

std::vector<std::string> WidgetManager::getLoadedWidgetsList()
{
    auto                     loaded = Settings::internal.loaded_widgets.GetValue();

    const std::regex         delimiter{","};
    std::vector<std::string> list(std::sregex_token_iterator(loaded.begin(), loaded.end(), delimiter, -1), {});

    return list;
}

void WidgetManager::saveLoadedWidgetsList(const std::vector<std::string>& list)
{
    std::string amend = std::accumulate(list.begin(), list.end(), std::string{}, [](std::string a, std::string b) {
        return a.empty() ? b : a + "," + b;
    });

    Settings::internal.loaded_widgets.SetValue(amend);
}
