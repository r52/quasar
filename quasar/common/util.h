#pragma once

#include <vector>

#include <QString>

namespace Util
{
    QString                  GetCommonAppDataPath();
    std::vector<std::string> SplitString(const std::string& src, const std::string& delimiter);
};  // namespace Util
