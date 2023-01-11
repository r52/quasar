#include "settings.h"

namespace Settings
{
    InternalSettings                                                                                                            internal;
    std::unordered_map<std::string, std::tuple<ExtensionInfo, std::vector<DataSourceSettings*>, std::vector<SettingsVariant>*>> extension;
}  // namespace Settings
