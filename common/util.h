#pragma once

#include "exports.h"

#include <vector>

#include <QString>

namespace Util
{
    common_API QString GetCommonAppDataPath();
    common_API std::vector<std::string> SplitString(const std::string& src, const std::string& delimiter);
};  // namespace Util
