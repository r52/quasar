/*! \file
    \brief Defines a std::hash specialization for QString

    By default, QString does not have a std::hash specialization implemented, so it
    doesn't work with C++11 containers such as std::unordered_map. This file provides
    an implementation to bridge the gap.
*/

#pragma once

#include <QHash>
#include <QString>

namespace std
{
    //! QString std::hash implementation
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
