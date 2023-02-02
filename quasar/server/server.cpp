#include "server.h"

#include <condition_variable>

#include "uwebsockets/App.h"

#include "common/config.h"
#include "common/qutil.h"
#include "common/settings.h"

#include "extension/extension.h"

#include "internal/ajax.h"
#include "internal/applauncher.h"

#include <QCoreApplication>
#include <QDir>
#include <QtNetworkAuth>

#include <spdlog/spdlog.h>

#include <fmt/core.h>
#include <jsoncons/json.hpp>

#define SEND_CLIENT_ERROR(d, ...)                 \
  sendErrorToClient(d, fmt::format(__VA_ARGS__)); \
  SPDLOG_WARN(__VA_ARGS__);

JSONCONS_N_MEMBER_TRAITS(ClientMsgParams, 0, topics, params, code, args);
JSONCONS_ALL_MEMBER_TRAITS(ClientMessage, method, params);
JSONCONS_ALL_MEMBER_TRAITS(ErrorOnlyMessage, errors);

using UWSSocket = uWS::WebSocket<false, true, PerSocketData>;

namespace
{
    // Server data
    uWS::App*               app          = nullptr;
    uWS::Loop*              loop         = nullptr;

    bool                    serverLoaded = false;
    std::mutex              serverMutex;
    std::condition_variable scv;

    std::set<std::string>   authcodes;
    std::mutex              authMutex;
}  // namespace

Server::Server(std::shared_ptr<Config> cfg) :
    methods{
        {"subscribe", std::bind(&Server::handleMethodSubscribe, this, std::placeholders::_1, std::placeholders::_2)},
        {    "query",     std::bind(&Server::handleMethodQuery, this, std::placeholders::_1, std::placeholders::_2)},
        {     "auth",      std::bind(&Server::handleMethodAuth, this, std::placeholders::_1, std::placeholders::_2)},
},
    config{cfg}
{
    using namespace std::literals;
    websocketServer = std::jthread{[this]() {
        loop = uWS::Loop::get();
        app  = new uWS::App();

        app->ws<PerSocketData>("/*",
               {/* Settings */
                   .maxPayloadLength = 16 * 1024,
                   .idleTimeout      = 0,
                   .maxBackpressure  = 1 * 1024 * 1024,
                   .maxLifetime      = 0,
                   /* Handlers */
                   .upgrade =
                       [](auto* res, auto* req, auto* context) {
                           res->template upgrade<PerSocketData>({},
                               req->getHeader("sec-websocket-key"),
                               req->getHeader("sec-websocket-protocol"),
                               req->getHeader("sec-websocket-extensions"),
                               context);
                       },
                   .open =
                       [this](UWSSocket* ws) {
                           auto data    = ws->getUserData();
                           data->socket = ws;

                           SPDLOG_INFO("New client connected!");

                           if (Settings::internal.auth.GetValue())
                           {
                               RunOnPool([data, this] {
                                   // Check for auth status after 10s
                                   std::this_thread::sleep_for(10s);
                                   if (!data->authenticated)
                                   {
                                       RunOnServer([=]() {
                                           auto socket = static_cast<UWSSocket*>(data->socket);
                                           socket->end(0, "Unauthenticated client");
                                       });
                                   }
                               });
                           }
                       },
                   .message =
                       [this](UWSSocket* ws, std::string_view message, uWS::OpCode opCode) {
                           RunOnPool([data = ws->getUserData(), this, msg = std::string{message}] {
                               this->processMessage(data, msg);
                           });
                       },
                   .subscription =
                       [this](UWSSocket* ws, std::string_view topic, int nSize, int oSize) {
                           this->processSubscription(ws->getUserData(), std::string{topic}, nSize, oSize);
                       },
                   .close =
                       [this](UWSSocket* ws, int code, std::string_view message) {
                           auto data = ws->getUserData();

                           this->processClose(data);

                           SPDLOG_INFO("Client disconnected.");
                       }})
            .listen("127.0.0.1",
                Settings::internal.port.GetValue(),
                [](auto* socket) {
                    if (socket)
                    {
                        SPDLOG_INFO("WebSocket Thread listening on port {}", Settings::internal.port.GetValue());

                        {
                            std::lock_guard lk(serverMutex);
                            serverLoaded = true;
                        }
                        scv.notify_one();
                    }
                    else
                    {
                        SPDLOG_ERROR("WebSocket Thread failed to listen on port {}", Settings::internal.port.GetValue());
                    }
                })
            .run();

        delete app;
        loop->free();
    }};

    {
        std::unique_lock lk(serverMutex);
        scv.wait(lk, [] {
            return serverLoaded;
        });
    }

    this->loadExtensions();

    // Force QtNetworkAuth linkage
    QOAuth2AuthorizationCodeFlow oauth2;
}

Server::~Server()
{
    loop->defer([]() {
        app->close();
    });

    pool.wait_for_tasks();

    websocketServer.join();

    extensions.clear();
}

bool Server::FindExtension(const std::string& extcode)
{
    std::shared_lock<std::shared_mutex> lk(extensionMutex);
    return (extensions.count(extcode) > 0);
}

void Server::SendDataToClient(PerSocketData* client, const std::string& msg)
{
    auto socket = static_cast<UWSSocket*>(client->socket);

    RunOnServer([=]() {
        socket->send(msg, uWS::TEXT);
    });
}

void Server::PublishData(std::string_view topic, const std::string& data)
{
    RunOnServer([=]() {
        app->publish(topic, data, uWS::TEXT);
    });
}

void Server::RunOnServer(auto&& cb)
{
    loop->defer(cb);
}

void Server::UpdateSettings()
{
    RunOnServer([=, this] {
        std::lock_guard<std::shared_mutex> lk(extensionMutex);
        for (auto& [name, ext] : extensions)
        {
            ext->UpdateExtensionSettings();
        }
    });
}

std::string Server::GenerateAuthCode()
{
    if (!Settings::internal.auth.GetValue())
    {
        return std::string{"dummycode"};
    }

    quint64 h   = QRandomGenerator::global()->generate64();
    quint64 l   = QRandomGenerator::global()->generate64();
    QString hv  = QString::number(h, 16).toUpper() + QString::number(l, 16).toUpper();

    auto    str = hv.toStdString();

    authcodes.insert(str);

    return str;
}

void Server::loadExtensions()
{
    const auto libTypes = QStringList() << "*.dll"
                                        << "*.so"
                                        << "*.dylib";
    auto          path = QUtil::GetCommonAppDataPath() + "extensions/";

    QDir          dir(path);
    QFileInfoList list = dir.entryInfoList(libTypes, QDir::Files);

    // Also check executable path
    auto extdir = QDir(QCoreApplication::applicationDirPath() + "/extensions/");
    list.append(extdir.entryInfoList(libTypes, QDir::Files));

    if (list.count() == 0)
    {
        SPDLOG_INFO("No data extensions found");
        return;
    }

    // just lock the whole thing while initializing extensions at startup
    // to prevent out of order reads
    {
        std::lock_guard<std::shared_mutex> lk(extensionMutex);

        // First load internal extensions
        {
            // App Launcher
            std::string name = "applauncher";
            Extension*  extn = Extension::LoadInternal(name, applauncher_load, applauncher_destroy, config.lock(), this);

            if (!extn)
            {
                SPDLOG_WARN("Failed to load extension {}", name);
            }
            else if (extensions.count(extn->GetName()))
            {
                SPDLOG_WARN("Extension with code {} already loaded. Unloading {}", extn->GetName(), name);
            }
            else
            {
                try
                {
                    extn->Initialize();
                } catch (std::exception e)
                {
                    SPDLOG_WARN("Exception: {} while initializing {}", e.what(), name);
                    delete extn;
                    extn = nullptr;
                }

                SPDLOG_INFO("Extension {} loaded.", extn->GetName());
                extensions[extn->GetName()].reset(extn);
                extn = nullptr;
            }

            if (extn != nullptr)
            {
                delete extn;
            }
        }

        {
            // AJAX
            std::string name = "ajax";
            Extension*  extn = Extension::LoadInternal(name, ajax_load, ajax_destroy, config.lock(), this);

            if (!extn)
            {
                SPDLOG_WARN("Failed to load extension {}", name);
            }
            else if (extensions.count(extn->GetName()))
            {
                SPDLOG_WARN("Extension with code {} already loaded. Unloading {}", extn->GetName(), name);
            }
            else
            {
                try
                {
                    extn->Initialize();
                } catch (std::exception e)
                {
                    SPDLOG_WARN("Exception: {} while initializing {}", e.what(), name);
                    delete extn;
                    extn = nullptr;
                }

                SPDLOG_INFO("Extension {} loaded.", extn->GetName());
                extensions[extn->GetName()].reset(extn);
                extn = nullptr;
            }

            if (extn != nullptr)
            {
                delete extn;
            }
        }

        // Load Extension libraries
        for (QFileInfo& file : list)
        {
            auto libpath = (file.path() + "/" + file.fileName()).toStdString();

            SPDLOG_INFO("Loading data extension {}", libpath);

            Extension* extn = Extension::Load(libpath, config.lock(), this);

            if (!extn)
            {
                SPDLOG_WARN("Failed to load extension {}", libpath);
            }
            else if (extensions.count(extn->GetName()))
            {
                SPDLOG_WARN("Extension with code {} already loaded. Unloading {}", extn->GetName(), libpath);
            }
            else
            {
                try
                {
                    extn->Initialize();
                } catch (std::exception e)
                {
                    SPDLOG_WARN("Exception: {} while initializing {}", e.what(), libpath);
                    delete extn;
                    extn = nullptr;
                    continue;
                }

                SPDLOG_INFO("Extension {} loaded.", extn->GetName());
                extensions[extn->GetName()].reset(extn);
                extn = nullptr;
            }

            if (extn != nullptr)
            {
                delete extn;
            }
        }

        SPDLOG_INFO("Extensions loaded!");
    }
}

void Server::handleMethodSubscribe(PerSocketData* client, const ClientMessage& msg)
{
    if (Settings::internal.auth.GetValue() and !client->authenticated)
    {
        SEND_CLIENT_ERROR(client, "Unauthenticated client");
        return;
    }

    auto& parms = msg.params;

    if (!parms.topics)
    {
        SEND_CLIENT_ERROR(client, "Invalid parameters for method 'subscribe'");
        return;
    }

    if (parms.topics.value().empty())
    {
        SEND_CLIENT_ERROR(client, "Invalid topics for method 'subscribe'");
        return;
    }

    auto&                               topics = parms.topics.value();

    std::shared_lock<std::shared_mutex> lk(extensionMutex);

    for (auto& topic : topics)
    {
        auto target = topic.substr(0, topic.find_first_of("/"));

        if (!extensions.count(target))
        {
            SEND_CLIENT_ERROR(client, "Unknown extension '{}' in topic {}", target, topic);
            continue;
        }

        auto extn = extensions[target].get();

        if (!extn->TopicExists(topic))
        {
            SEND_CLIENT_ERROR(client, "Nonexistent topic '{}'", topic);
            continue;
        }

        if (!extn->TopicAcceptsSubscribers(topic))
        {
            SEND_CLIENT_ERROR(client, "Topic '{}' does not accept subscribers", topic);
            continue;
        }

        auto socket = static_cast<UWSSocket*>(client->socket);

        RunOnServer([=, this]() {
            auto res = socket->subscribe(topic);

            if (res)
            {
                SPDLOG_INFO("Widget subscribed to topic {}", topic);
            }
            else
            {
                SEND_CLIENT_ERROR(client, "Failed to subscribed to topic {}", topic);
            }
        });
    }
}

void Server::handleMethodQuery(PerSocketData* client, const ClientMessage& msg)
{
    if (Settings::internal.auth.GetValue() and !client->authenticated)
    {
        SEND_CLIENT_ERROR(client, "Unauthenticated client");
        return;
    }

    auto& parms = msg.params;

    if (!parms.topics)
    {
        SEND_CLIENT_ERROR(client, "Invalid parameters for method 'query'");
        return;
    }

    if (parms.topics.value().empty())
    {
        SEND_CLIENT_ERROR(client, "Invalid topics for method 'query'");
        return;
    }

    auto&                                                     topics = parms.topics.value();
    auto                                                      params = parms.params ? parms.params.value() : std::vector<std::string>{};
    auto                                                      args   = parms.args ? parms.args.value() : "";

    std::unordered_map<std::string, std::vector<std::string>> extns{};

    for (auto& topic : topics)
    {
        auto target = topic.substr(0, topic.find_first_of("/"));
        extns[target].push_back(topic);
    }

    std::shared_lock<std::shared_mutex> lk(extensionMutex);

    jsoncons::json                      j{jsoncons::json_object_arg, {{"errors", jsoncons::json{jsoncons::json_array_arg}}}};
    std::string                         message{};

    for (auto& [target, tpcs] : extns)
    {
        if (!extensions.count(target))
        {
            SEND_CLIENT_ERROR(client, "Unknown extension {} in topic {}", target, fmt::join(tpcs, ","));
            continue;
        }

        auto extn = extensions[target].get();

        extn->PollDataForSending(j, tpcs, args, client);
    }

    if (j["errors"].empty())
    {
        j.erase("errors");
    }

    if (!j.empty())
    {
        j.dump(message);
        SendDataToClient(client, message);
    }
}

void Server::handleMethodAuth(PerSocketData* client, const ClientMessage& msg)
{
    if (!Settings::internal.auth.GetValue())
    {
        SPDLOG_INFO("Widget authentication is disabled");
        return;
    }

    if (client->authenticated)
    {
        SEND_CLIENT_ERROR(client, "Client already authenticated.");
        return;
    }

    auto& parms = msg.params;

    if (!parms.code)
    {
        SEND_CLIENT_ERROR(client, "Invalid parameters for method 'auth'");
        return;
    }

    if (parms.code.value().empty())
    {
        SEND_CLIENT_ERROR(client, "Invalid auth code for method 'auth'");
        return;
    }

    auto&                       code = parms.code.value();

    std::lock_guard<std::mutex> lk(authMutex);

    auto                        search = authcodes.find(code);
    if (search == authcodes.end())
    {
        SEND_CLIENT_ERROR(client, "Invalid authentication code");
        return;
    }

    authcodes.erase(search);

    client->authenticated = true;
    SPDLOG_INFO("Client authenticated!");
}

void Server::processMessage(PerSocketData* client, const std::string& msg)
{
    ClientMessage doc{};

    try
    {
        doc = jsoncons::decode_json<ClientMessage>(msg);
    } catch (std::exception const& je)
    {
        SPDLOG_ERROR("Error parsing JSON message: {}", je.what());
        SPDLOG_ERROR("JSON: {}", msg);
        return;
    }

    if (doc.method.empty())
    {
        SEND_CLIENT_ERROR(client, "Invalid JSON request");
        return;
    }

    if (!methods.count(doc.method))
    {
        SEND_CLIENT_ERROR(client, "Unknown method type {}", doc.method);
        return;
    }

    methods[doc.method](client, doc);
}

void Server::sendErrorToClient(PerSocketData* client, const std::string& err)
{
    // Craft error json msg
    ErrorOnlyMessage msg{.errors = {{err}}};
    std::string      json{};

    jsoncons::encode_json(msg, json);

    SendDataToClient(client, json);
}

void Server::processClose(PerSocketData* client)
{
    // Currently nothing
}

void Server::processSubscription(PerSocketData* client, const std::string& topic, int nSize, int oSize)
{
    std::shared_lock<std::shared_mutex> lk(extensionMutex);

    auto                                target = topic.substr(0, topic.find_first_of("/"));

    if (!extensions.count(target))
    {
        SEND_CLIENT_ERROR(client, "Unknown extension '{}' in topic {}", target, topic);
        return;
    }

    auto extn = extensions[target].get();

    if (nSize > oSize)
    {
        // New subscriber

        extn->AddSubscriber(client, topic, nSize);
    }
    else if (nSize < oSize)
    {
        // Remove subscriber
        extn->RemoveSubscriber(client, topic, nSize);
    }
    else
    {
        // Undefined behaviour
        SPDLOG_WARN("Undefined subscription behaviour");
    }
}
