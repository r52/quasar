#include "protocol.h"

ClientMessage parse_client_message(std::string_view json_document)
{
    return daw::json::from_json<ClientMessage>(json_document, daw::json::options::parse_flags<daw::json::options::PolicyCommentTypes::cpp>);
}

ServerMessage parse_server_message(std::string_view json_document)
{
    return daw::json::from_json<ServerMessage>(json_document, daw::json::options::parse_flags<daw::json::options::PolicyCommentTypes::cpp>);
}
