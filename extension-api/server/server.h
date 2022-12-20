#pragma once

#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>

#include "extension_exports.h"

class Extension;
class Config;

struct PerSocketData
{
    void* socket = nullptr;
};

class extension_API Server : public std::enable_shared_from_this<Server>
{
    using ExtensionsMapType = std::unordered_map<std::string, std::unique_ptr<Extension>>;

public:
    Server(std::shared_ptr<Config> cfg);
    ~Server();

    bool FindExtension(const std::string& extcode);

private:
    void                      loadExtensions();

    void                      processMessage(PerSocketData* dat, const std::string& msg);
    void                      sendErrorToClient(PerSocketData* dat, const std::string& err);

    std::jthread              websocketServer;

    ExtensionsMapType         extensions;
    mutable std::shared_mutex extensionMutex;

    std::weak_ptr<Config>     config{};
};
