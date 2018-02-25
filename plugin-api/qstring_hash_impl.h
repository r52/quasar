#pragma once

#include <QHash>
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
            return static_cast<result_type>(qHash(s));
        }
    };
}
