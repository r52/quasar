#include "server.h"

#include "uwebsockets/App.h"

#include "config.h"
#include "settings.h"
#include "util.h"

#include "extension.h"

#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include <QUrl>

#include <BS_thread_pool.hpp>
#include <glaze/glaze.hpp>
#include <spdlog/spdlog.h>

#include <fmt/core.h>
#include <glaze/core/macros.hpp>

#define SEND_CLIENT_ERROR(d, ...)                 \
  sendErrorToClient(d, fmt::format(__VA_ARGS__)); \
  SPDLOG_WARN(__VA_ARGS__);

GLZ_META(ClientMsgParams, topics, params, code, args);
GLZ_META(ClientMessage, method, params);
GLZ_META(ErrorOnlyMessage, errors);

using UWSSocket = uWS::WebSocket<false, true, PerSocketData>;

namespace
{
    // Server data
    uWS::App*       app  = nullptr;
    uWS::Loop*      loop = nullptr;
    BS::thread_pool pool;
}  // namespace

Server::Server(std::shared_ptr<Config> cfg) :
    config{
        cfg
},
    methods{
        {"subscribe", std::bind(&Server::handleMethodSubscribe, this, std::placeholders::_1, std::placeholders::_2)},
        {"query", std::bind(&Server::handleMethodQuery, this, std::placeholders::_1, std::placeholders::_2)},
    }
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
                       [](UWSSocket* ws) {
                           auto data    = ws->getUserData();
                           data->socket = ws;

                           SPDLOG_INFO("New client connected!");
                       },
                   .message =
                       [this](UWSSocket* ws, std::string_view message, uWS::OpCode opCode) {
                           pool.push_task([data = ws->getUserData(), this, msg = std::string{message}] {
                               this->processMessage(data, msg);
                           });
                       },
                   .subscription =
                       [this](UWSSocket* ws, std::string_view topic, int nSize, int oSize) {
                           pool.push_task([=, data = ws->getUserData(), this, tpc = std::string{topic}] {
                               this->processSubscription(data, tpc, nSize, oSize);
                           });
                       },
                   .close =
                       [this](UWSSocket* ws, int code, std::string_view message) {
                           auto data = ws->getUserData();

                           this->processClose(data);

                           SPDLOG_INFO("Client disconnected.");
                       }})
            .listen(Settings::internal.port.GetValue(),
                [this](auto* socket) {
                    if (socket)
                    {
                        SPDLOG_INFO("WebSocket Thread listening on port {}", Settings::internal.port.GetValue());

                        pool.push_task([this] {
                            this->loadExtensions();
                        });
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
}

Server::~Server()
{
    loop->defer([]() {
        app->close();
    });
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

void Server::PublishData(const std::string& topic, const std::string& data)
{
    RunOnServer([=]() {
        app->publish(topic, data, uWS::TEXT);
    });
}

void Server::RunOnServer(auto&& cb)
{
    loop->defer(cb);
}

void Server::loadExtensions()
{
    auto          path = Util::GetCommonAppDataPath() + "extensions/";

    QDir          dir(path);
    QFileInfoList list = dir.entryInfoList(QStringList() << "*.dll"
                                                         << "*.so"
                                                         << "*.dylib",
        QDir::Files);

    // Also check executable path
    auto extdir = QDir(QCoreApplication::applicationDirPath() + "/extensions/");
    list.append(extdir.entryInfoList(QStringList() << "*.dll"
                                                   << "*.so"
                                                   << "*.dylib",
        QDir::Files));

    if (list.count() == 0)
    {
        SPDLOG_INFO("No data extensions found");
        return;
    }

    // just lock the whole thing while initializing extensions at startup
    // to prevent out of order reads
    std::lock_guard<std::shared_mutex> lk(extensionMutex);

    for (QFileInfo& file : list)
    {
        auto libpath = (file.path() + "/" + file.fileName()).toStdString();

        SPDLOG_INFO("Loading data extension {}", libpath);

        Extension* extn = Extension::Load(libpath, config.lock(), shared_from_this());

        if (!extn)
        {
            SPDLOG_WARN("Failed to load extension {}", libpath);
        }
        // TODO internal targets
        // else if (m_InternalQueryTargets.count(extn->GetName()))
        // {
        //     SPDLOG_WARN("The extension code {} is reserved. Unloading {}", extn->GetName(), libpath);
        // }
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
}

void Server::handleMethodSubscribe(PerSocketData* client, const ClientMessage& msg)
{
    // TODO client auth?
    // {
    //     std::shared_lock<std::shared_mutex> lk(m_AuthedClientsMtx);

    //     auto it = m_AuthedClientsSet.find(sender);
    //     if (it == m_AuthedClientsSet.end())
    //     {
    //         DS_SEND_WARN(sender, "Unauthenticated client");
    //         return;
    //     }
    // }

    // auto qvar = sender->property(WGT_PROP_IDENTITY);
    // if (!qvar.isValid())
    // {
    //     qCritical() << "Invalid identity in authenticated WebSocket connection.";
    //     return;
    // }

    // client_data_t clidat     = qvar.value<client_data_t>();
    // QString       widgetName = clidat.ident;

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
            SEND_CLIENT_ERROR(client, "Unknown extension '{}'", target);
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

        loop->defer([=, this]() {
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
    // TODO client auth?
    // {
    //     std::shared_lock<std::shared_mutex> lk(m_AuthedClientsMtx);

    //     auto it = m_AuthedClientsSet.find(sender);
    //     if (it == m_AuthedClientsSet.end())
    //     {
    //         DS_SEND_WARN(sender, "Unauthenticated client");
    //         return;
    //     }
    // }

    // auto qvar = sender->property(WGT_PROP_IDENTITY);
    // if (!qvar.isValid())
    // {
    //     qCritical() << "Invalid identity in authenticated WebSocket connection.";
    //     return;
    // }

    // client_data_t clidat = qvar.value<client_data_t>();

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

    auto& topics = parms.topics.value();
    auto  params = parms.params ? parms.params.value() : std::vector<std::string>{};
    auto  args   = parms.args ? parms.args.value() : "";

    // TODO internal targets
    // if (m_InternalQueryTargets.count(extcode))
    // {
    //     m_InternalQueryTargets[extcode](extparm, clidat, sender);
    //     return;
    // }

    std::unordered_map<std::string, std::vector<std::string>> extns{};

    for (auto& topic : topics)
    {
        auto target = topic.substr(0, topic.find_first_of("/"));
        extns[target].push_back(topic);
    }

    std::shared_lock<std::shared_mutex> lk(extensionMutex);

    jsoncons::json                      j{jsoncons::json_object_arg, {{"errors", jsoncons::json{jsoncons::json_array_arg}}}};
    std::string                         message{};

    for (auto& ex : extns)
    {
        auto& target = ex.first;
        auto& tpcs   = ex.second;

        if (!extensions.count(target))
        {
            SEND_CLIENT_ERROR(client, "Unknown extension {}", target);
            continue;
        }

        auto extn = extensions[target].get();

        extn->PollDataForSending(j, tpcs, args, client);
    }

    j.dump(message);

    if (!message.empty())
    {
        SendDataToClient(client, message);
    }
}

void Server::processMessage(PerSocketData* client, const std::string& msg)
{
    // SPDLOG_DEBUG("Raw WebSocket message: {}", msg);

    ClientMessage doc{};

    try
    {
        glz::read_json(doc, msg);
    } catch (std::exception const& je)
    {
        SPDLOG_ERROR("Error parsing JSON message: {}", je.what());
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
    glz::write_json(msg, json);

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
        SEND_CLIENT_ERROR(client, "Unknown extension '{}'", target);
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
