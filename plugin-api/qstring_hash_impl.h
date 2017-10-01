#pragma once

#include <QString>

namespace std
{
    template <>
    struct hash<QString>
    {
        typedef QString     argument_type;
        typedef std::size_t result_type;
        result_type         operator()(argument_type const& s) const
        {
            return std::hash<std::string>{}(s.toStdString());
        }
    };
}
