#include "util.h"

#include <algorithm>
#include <regex>

std::vector<std::string> Util::SplitString(const std::string& src, const std::string& delimiter)
{
    const std::regex         del{delimiter};
    std::vector<std::string> list(std::sregex_token_iterator(src.begin(), src.end(), del, -1), {});

    std::erase_if(list, [](const std::string& s) {
        return s.empty();
    });

    return list;
}

char* Util::SafeCStrCopy(char* dest, size_t destSize, const char* src, size_t srcSize)
{
    if (destSize > 0 and srcSize > 0)
    {
        size_t i;
        for (i = 0; i < destSize - 1 and i < srcSize and src[i]; i++)
        {
            dest[i] = src[i];
        }
        dest[i] = '\0';
    }
    return dest;
}
