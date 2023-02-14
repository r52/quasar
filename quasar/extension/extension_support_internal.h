/*! \file
    \brief Internal types for processing extension_support.h functions. Not a part of API.
    This file is **NOT** a part of the Extension API and is not shipped with the
    API package. For reference only. This file is subjected to major changes.
*/

#pragma once

#include <optional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include "common/settings.h"

#include <jsoncons/json.hpp>

using SelectionOptionsVector = std::vector<std::pair<std::string, std::string>>;

//! Internal struct holding return value and any errors
struct quasar_return_data_t
{
    std::optional<jsoncons::json> val;     //!< Return value
    std::vector<std::string>      errors;  //!< Array of errors
};
