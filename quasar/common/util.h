#pragma once

#include <string>
#include <vector>

namespace Util
{

    std::vector<std::string> SplitString(const std::string& src, const std::string& delimiter);
    char*                    SafeCStrCopy(char* dest, size_t destSize, const char* src, size_t srcSize);
};  // namespace Util
