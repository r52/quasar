#pragma once

#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>

class Server;
class Config;
class QuasarWidget;

using WidgetMapType = std::unordered_map<std::string, std::unique_ptr<QuasarWidget>>;

class WidgetManager : public std::enable_shared_from_this<WidgetManager>
{
public:
    WidgetManager(std::shared_ptr<Server> serv);
    ~WidgetManager();

    bool                       LoadWidget(std::string filename, std::shared_ptr<Config> config, bool userAction);
    void                       CloseWidget(QuasarWidget* widget);

    void                       LoadStartupWidgets(std::shared_ptr<Config> config);

    std::vector<QuasarWidget*> GetWidgets();

private:
    std::vector<std::string>  getLoadedWidgetsList();
    void                      saveLoadedWidgetsList(const std::vector<std::string>& list);

    WidgetMapType             widgetMap;
    std::weak_ptr<Server>     server;

    mutable std::shared_mutex m_mutex;
};
