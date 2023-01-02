#include "settings.h"

namespace Settings
{
    InternalSettings                                     internal;
    std::unordered_map<std::string, DataSourceSettings*> datasource;
}  // namespace Settings
