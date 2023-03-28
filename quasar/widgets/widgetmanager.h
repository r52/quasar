#pragma once

#include "widgetdefinition.h"

#include <functional>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>

class Server;
class Config;
class QuasarWidget;

using WidgetMapType         = std::unordered_map<std::string, std::unique_ptr<QuasarWidget>>;
using WidgetChangedCallback = std::function<void(const std::vector<QuasarWidget*>&)>;

class WidgetManager : public std::enable_shared_from_this<WidgetManager>
{
public:
    WidgetManager(const WidgetManager&)             = delete;
    WidgetManager& operator= (const WidgetManager&) = delete;

    WidgetManager(std::shared_ptr<Server> serv, std::shared_ptr<Config> cfg);
    ~WidgetManager();

    bool                       LoadWidget(const std::string& filename, bool userAction);
    void                       CloseWidget(QuasarWidget* widget);

    void                       LoadStartupWidgets();

    std::vector<QuasarWidget*> GetWidgets();

    void                       SetWidgetChangedCallback(WidgetChangedCallback&& cb);

private:
    bool                      acceptSecurityWarnings(const WidgetDefinition& def);
    std::vector<std::string>  getLoadedWidgetsList();
    void                      saveLoadedWidgetsList(const std::vector<std::string>& list);

    WidgetMapType             widgetMap;
    WidgetChangedCallback     widgetChangedCb;

    std::weak_ptr<Server>     server{};
    std::weak_ptr<Config>     config{};

    mutable std::shared_mutex mutex;
};
