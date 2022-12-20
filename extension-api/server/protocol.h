#pragma once

#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include <daw/json/daw_json_link.h>

struct ClientMsgParams
{
    std::optional<std::string>              target;
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

ClientMessage parse_client_message(std::string_view json_document);

struct ServerMessage
{
    std::optional<std::string>              data;
    std::optional<std::vector<std::string>> errors;
};

ServerMessage parse_server_message(std::string_view json_document);

namespace daw::json
{
    template<>
    struct json_data_contract<ClientMsgParams>
    {
        using type = json_member_list<json_string_null<"target">,
            json_array_null<"params", std::optional<std::vector<std::string>>>,
            json_string_null<"code">,
            json_raw_null<"args", std::string>>;

        static inline auto to_json_data(ClientMsgParams const& value) { return std::forward_as_tuple(value.target, value.params, value.code, value.args); }
    };

    template<>
    struct json_data_contract<ClientMessage>
    {
        using type = json_member_list<json_string<"method">, json_class<"params", ClientMsgParams>>;

        static inline auto to_json_data(ClientMessage const& value) { return std::forward_as_tuple(value.method, value.params); }
    };

    template<>
    struct json_data_contract<ServerMessage>
    {
        using type = json_member_list<json_raw_null<"data", std::string>, json_array_null<"errors", std::optional<std::vector<std::string>>>>;

        static inline auto to_json_data(ServerMessage const& value) { return std::forward_as_tuple(value.data, value.errors); }
    };
};  // namespace daw::json
