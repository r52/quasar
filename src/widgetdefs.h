#pragma once

constexpr auto WGT_DEF_NAME          = "name";
constexpr auto WGT_DEF_WIDTH         = "width";
constexpr auto WGT_DEF_HEIGHT        = "height";
constexpr auto WGT_DEF_STARTFILE     = "startFile";
constexpr auto WGT_DEF_FULLPATH      = "fullPath";
constexpr auto WGT_DEF_TRANSPARENTBG = "transparentBg";
constexpr auto WGT_DEF_REMOTEACCESS  = "remoteAccess";
constexpr auto WGT_DEF_CLICKABLE     = "clickable";
constexpr auto WGT_DEF_REQUIRED      = "required";
constexpr auto WGT_DEF_OPTIONAL      = "optional";
constexpr auto WGT_DEF_DATASERVER    = "dataserver";

constexpr auto WGT_PROP_IDENTITY = "qsr_identity";
constexpr auto WGT_PROP_USERKEY  = "qsr_userkey";

constexpr auto QUASAR_CONFIG_SECURE      = "global/secure";
constexpr auto QUASAR_CONFIG_PORT        = "global/dataport";
constexpr auto QUASAR_CONFIG_LOADED      = "global/loaded";
constexpr auto QUASAR_CONFIG_LOGLEVEL    = "global/loglevel";
constexpr auto QUASAR_CONFIG_LOGFILE     = "global/logfile";
constexpr auto QUASAR_CONFIG_COOKIES     = "global/cookies";
constexpr auto QUASAR_CONFIG_ALLOWGEO    = "global/allowGeo";
constexpr auto QUASAR_CONFIG_LASTPATH    = "global/lastpath";
constexpr auto QUASAR_CONFIG_LAUNCHERMAP = "launcher/map";
constexpr auto QUASAR_CONFIG_USERKEYSMAP = "userkeys/map";

constexpr auto QUASAR_DATA_SERVER_DEFAULT_PORT = 13337;

#define QUASAR_CONFIG_DEFAULT_LOGLEVEL QUASAR_LOG_WARNING
