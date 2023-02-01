#pragma once

#include <functional>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>

#include "protocol.h"

#include <BS_thread_pool.hpp>

class Extension;
class Config;

struct PerSocketData
{
    void* socket        = nullptr;
    bool  authenticated = false;
};

class Server : public std::enable_shared_from_this<Server>
{
    using ExtensionsMapType = std::unordered_map<std::string, std::unique_ptr<Extension>>;
    using MethodFuncType    = std::function<void(PerSocketData*, const ClientMessage&)>;
    using MethodCallMapType = std::unordered_map<std::string, MethodFuncType>;

public:
    Server(const Server&)             = delete;
    Server& operator= (const Server&) = delete;

    Server(std::shared_ptr<Config> cfg);
    ~Server();

    bool        FindExtension(const std::string& extcode);

    void        SendDataToClient(PerSocketData* client, const std::string& msg);

    void        PublishData(std::string_view topic, const std::string& data);

    void        RunOnServer(auto&& cb);

    void        RunOnPool(auto&& cb) { pool.push_task(cb); }

    void        UpdateSettings();

    std::string GenerateAuthCode();

private:
    void loadExtensions();

    // Method handling
    void         handleMethodSubscribe(PerSocketData* client, const ClientMessage& msg);
    void         handleMethodQuery(PerSocketData* client, const ClientMessage& msg);
    void         handleMethodAuth(PerSocketData* client, const ClientMessage& msg);

    void         processMessage(PerSocketData* client, const std::string& msg);
    void         sendErrorToClient(PerSocketData* client, const std::string& err);
    void         processClose(PerSocketData* client);
    void         processSubscription(PerSocketData* client, const std::string& topic, int nSize, int oSize);

    std::jthread websocketServer;

    // Method function map
    MethodCallMapType         methods;

    ExtensionsMapType         extensions;
    mutable std::shared_mutex extensionMutex;

    std::weak_ptr<Config>     config{};

    BS::thread_pool           pool;
};
