#pragma once

#include <tuple>

#include <daw/json/daw_json_link.h>

namespace daw::json
{
    template<>
    struct json_data_contract<WidgetDefinition>
    {
        using type = json_member_list<json_string<"name">,
                                      json_number<"width", unsigned>,
                                      json_number<"height", unsigned>,
                                      json_string<"startFile">,
                                      json_bool<"transparentBg">,
                                      json_bool_null<"clickable">,
                                      json_bool_null<"dataserver">,
                                      json_bool_null<"remoteAccess">,
                                      json_array_null<"required", std::optional<std::vector<std::string>>>>;

        static inline auto to_json_data(WidgetDefinition const& value)
        {
            return std::forward_as_tuple(value.name,
                                         value.width,
                                         value.height,
                                         value.startFile,
                                         value.transparentBg,
                                         value.clickable,
                                         value.dataserver,
                                         value.remoteAccess,
                                         value.required);
        }
    };
};  // namespace daw::json
