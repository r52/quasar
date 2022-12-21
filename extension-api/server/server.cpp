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

#include <glaze/core/macros.hpp>

#define SEND_CLIENT_ERROR(d, e) \
  sendErrorToClient(d, e);      \
  SPDLOG_WARN(e);

GLZ_META(ClientMsgParams, target, params, code, args);
GLZ_META(ClientMessage, method, params);
GLZ_META(ServerMessage, data, errors);

using UWSSocket = uWS::WebSocket<false, true, PerSocketData>;

namespace
{
    // Server data
    uWS::App*                      app           = nullptr;
    uWS::Loop*                     loop          = nullptr;
    us_listen_socket_t*            listen_socket = nullptr;
    std::unordered_set<UWSSocket*> connections;
    const int                      _SSL = 0;
    BS::thread_pool                pool;
    constexpr int                  shutdownCode = 4444;
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

        std::this_thread::sleep_for(1s);

        app->ws<PerSocketData>("/*",
               {/* Settings */
                   .maxPayloadLength = 16 * 1024,
                   .idleTimeout      = 0,
                   .maxBackpressure  = 1 * 1024 * 1024,
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

                           connections.insert(ws);

                           SPDLOG_INFO("New client connected!");
                       },
                   .message =
                       [this](UWSSocket* ws, std::string_view message, uWS::OpCode opCode) {
                           pool.push_task([data = ws->getUserData(), this, msg = std::string{message}] {
                               this->processMessage(data, msg);
                           });
                       },
                   .close =
                       [](UWSSocket* ws, int code, std::string_view message) {
                           // Unnecessary cleanup, but just in case
                           auto data    = ws->getUserData();
                           data->socket = nullptr;

                           if (code != shutdownCode)
                           {
                               // This gets called during socket closure at destruction time as well
                               // so don't modify the container if called during exit cleanup
                               connections.erase(ws);
                           }
                       }})
            .listen(Settings::internal.port.GetValue(),
                [this](auto* socket) {
                    if (socket)
                    {
                        listen_socket = socket;
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
        for (auto* s : connections)
        {
            s->end(shutdownCode);
        }

        if (listen_socket)
        {
            us_listen_socket_close(_SSL, listen_socket);
            listen_socket = nullptr;
        }
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

    loop->defer([=]() {
        socket->send(msg, uWS::TEXT);
    });
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
    // TODO subscribe
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

    if (!(parms.target and parms.params))
    {
        SEND_CLIENT_ERROR(client, "Invalid parameters for method 'query'");
        return;
    }

    if (parms.target.value().empty())
    {
        SEND_CLIENT_ERROR(client, "Invalid target for method 'query'");
        return;
    }

    auto& target = parms.target.value();
    auto& params = parms.params.value();
    auto  args   = parms.args ? parms.args.value() : "";

    // TODO internal targets
    // if (m_InternalQueryTargets.count(extcode))
    // {
    //     m_InternalQueryTargets[extcode](extparm, clidat, sender);
    //     return;
    // }

    std::shared_lock<std::shared_mutex> lk(extensionMutex);

    if (!extensions.count(target))
    {
        SEND_CLIENT_ERROR(client, "Unknown extension identifier " + target);
        return;
    }

    auto        extn   = extensions[target].get();

    std::string result = extn->PollDataForSending(params, args, client);

    if (!result.empty())
    {
        SendDataToClient(client, result);
    }
}

void Server::processMessage(PerSocketData* client, const std::string& msg)
{
    SPDLOG_DEBUG("Raw WebSocket message: {}", msg);

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
        SEND_CLIENT_ERROR(client, "Unknown method type " + doc.method);
        return;
    }

    methods[doc.method](client, doc);
}

void Server::sendErrorToClient(PerSocketData* client, const std::string& err)
{
    // Craft error json msg
    ServerMessage msg{.errors = {{err}}};
    std::string   json{};
    glz::write_json(msg, json);

    SendDataToClient(client, json);
}
