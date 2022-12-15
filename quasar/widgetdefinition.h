#pragma once

#include <optional>
#include <string>
#include <vector>

struct WidgetDefinition
{
    // Widget definition schema
    std::string                             name;
    std::size_t                             width;
    std::size_t                             height;
    std::string                             startFile;
    bool                                    transparentBg;
    std::optional<bool>                     clickable;
    std::optional<bool>                     dataserver;
    std::optional<bool>                     remoteAccess;
    std::optional<std::vector<std::string>> required;

    // Internal
    std::string fullpath;
};

WidgetDefinition parse_definition(std::string_view json_document);
