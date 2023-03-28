#pragma once

#include <optional>
#include <string>
#include <variant>
#include <vector>


struct ExtensionRequirement
{
    std::string name;
    std::string platform;
};

struct WidgetDefinition
{
    // Widget definition schema
    std::string                                                                 name;
    std::size_t                                                                 width;
    std::size_t                                                                 height;
    std::string                                                                 startFile;
    bool                                                                        transparentBg;
    std::optional<bool>                                                         clickable;
    std::optional<bool>                                                         dataserver;
    std::optional<bool>                                                         remoteAccess;
    std::optional<std::vector<std::variant<std::string, ExtensionRequirement>>> required;

    // Internal
    std::string fullpath;
};
