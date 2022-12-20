#pragma once

#include <functional>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>

#include "extension_exports.h"
#include "protocol.h"

class Extension;
class Config;

struct PerSocketData
{
    void* socket = nullptr;
};

class extension_API Server : public std::enable_shared_from_this<Server>
{
    using ExtensionsMapType = std::unordered_map<std::string, std::unique_ptr<Extension>>;
    using MethodFuncType    = std::function<void(PerSocketData*, const ClientMessage&)>;
    using MethodCallMapType = std::unordered_map<std::string, MethodFuncType>;

public:
    Server(std::shared_ptr<Config> cfg);
    ~Server();

    bool FindExtension(const std::string& extcode);

private:
    void loadExtensions();

    // Method handling
    void         handleMethodSubscribe(PerSocketData* data, const ClientMessage& msg);
    void         handleMethodQuery(PerSocketData* data, const ClientMessage& msg);

    void         processMessage(PerSocketData* dat, const std::string& msg);
    void         sendErrorToClient(PerSocketData* dat, const std::string& err);

    std::jthread websocketServer;

    // Method function map
    MethodCallMapType         methods;

    ExtensionsMapType         extensions;
    mutable std::shared_mutex extensionMutex;

    std::weak_ptr<Config>     config{};
};
