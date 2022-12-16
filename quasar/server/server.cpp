#include "server.h"

#include "protocol.h"
#include "settings.h"

#include "uwebsockets/App.h"

#include <BS_thread_pool.hpp>
#include <daw/json/daw_json_link.h>
#include <spdlog/spdlog.h>

#define SEND_CLIENT_ERROR(d, e) \
  sendErrorToClient(d, e);      \
  SPDLOG_WARN(e);

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

Server::Server()
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
                                .open =
                                    [](UWSSocket* ws) {
                                        auto data    = ws->getUserData();
                                        data->socket = ws;

                                        connections.insert(ws);
                                    },
                                .message =
                                    [this](UWSSocket* ws, std::string_view message, uWS::OpCode opCode) {
                                        pool.push_task([data = ws->getUserData(), this, msg = std::string{message}] {
                                            this->processMessage(data, msg);
                                        });
                                    },
                                .close =
                                    [](UWSSocket* ws, int code, std::string_view message) {
                                        if (code != shutdownCode)
                                        {
                                            // This gets called during socket closure at destruction time as well
                                            // so don't modify the container if called during exit cleanup
                                            connections.erase(ws);
                                        }
                                    }})
            .listen(Settings::internal.port.GetValue(),
                    [](auto* socket) {
                        if (socket)
                        {
                            listen_socket = socket;
                            SPDLOG_INFO("WebSocket Thread listening on port {}", Settings::internal.port.GetValue());
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
}

bool Server::FindExtension(std::string_view extcode)
{
    return false;
}

void Server::processMessage(PerSocketData* dat, const std::string& msg)
{
    SPDLOG_DEBUG("Raw WebSocket message: {}", msg);

    ClientMessage doc;

    try
    {
        doc = parse_client_message(msg);
    } catch (daw::json::json_exception const& je)
    {
        SPDLOG_ERROR("Error parsing JSON message: {}", to_formatted_string(je));
        return;
    }

    // TODO handle request
    if (doc.method.empty())
    {
        SEND_CLIENT_ERROR(dat, "Invalid JSON request");
        return;
    }

    // QString mtd = req["method"].toString();

    // if (!m_Methods.count(mtd))
    // {
    //     DS_SEND_WARN(sender, "Unknown method type " + mtd);
    //     return;
    // }

    // m_Methods[mtd](req, sender);
}

void Server::sendErrorToClient(PerSocketData* dat, const std::string& err)
{
    // Craft error json msg
    ServerMessage msg{.errors = {{err}}};
    std::string   json   = daw::json::to_json(msg);
    auto          socket = static_cast<UWSSocket*>(dat->socket);

    loop->defer([=]() {
        socket->send(json, uWS::OpCode::TEXT);
    });
}
