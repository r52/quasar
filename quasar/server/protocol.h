#pragma once

#include <optional>
#include <string>
#include <tuple>
#include <vector>

struct ClientMsgParams
{
    std::optional<std::vector<std::string>> topics;
    std::optional<std::vector<std::string>> params;
    std::optional<std::string>              code;
    std::optional<std::string>              args;
};

struct ClientMessage
{
    // Client message schema
    std::string     method;
    ClientMsgParams params;
};

struct ErrorOnlyMessage
{
    std::vector<std::string> errors;
};
