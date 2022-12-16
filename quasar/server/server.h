#pragma once

#include <string>
#include <thread>

struct PerSocketData
{
    void* socket = nullptr;
};

class Server
{
public:
    // TODO
    Server();
    ~Server();

    bool FindExtension(std::string_view extcode);

private:
    void         processMessage(PerSocketData* dat, const std::string& msg);
    void         sendErrorToClient(PerSocketData* dat, const std::string& err);

    std::jthread websocketServer;
};
