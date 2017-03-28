#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

    enum QuasarLogLevel
    {
        QUASAR_LOG_DEBUG,
        QUASAR_LOG_INFO,
        QUASAR_LOG_WARNING,
        QUASAR_LOG_CRITICAL
    };

    // Plugin init/de-init commands
    enum QuasarInitCmd
    {
        QUASAR_INIT_START = 0,          // Inits the plugin. Allocates memory, fills info struct etc
        QUASAR_INIT_RESP,               // App response
        QUASAR_INIT_SHUTDOWN            // Plugin shutdown
    };

    enum QuasarRetDataType
    {
        QUASAR_TREAT_AS_STRING = 0,
        QUASAR_TREAT_AS_JSON,
        QUASAR_TREAT_AS_BINARY
    };

    // Log function fp
    typedef void(*log_func_type)(int, const char*);

    struct QuasarPluginDataSource
    {
        char dataSrc[32];           // codename of this data source
        unsigned int refreshMsec;   // rate of refresh for this data source (0 means never; populated once and never changed)
        unsigned int uid;           // uid assigned to this data source by quasar
    };

    struct QuasarPluginInfo
    {
        // Sent by quasar
        log_func_type logFunc;

        // Filled by plugin
        char pluginName[64];                    // name of this plugin
        char pluginCode[16];                    // unique short code for widgets to identify and subscribe to this plugin
        char version[64];                       // version string
        char author[64];                        // author
        char description[256];                  // plugin description
        unsigned int numDataSources;            // number of data sources
        QuasarPluginDataSource* dataSources;    // list of data sources this plugin offers
    };

#if defined(__cplusplus)
}
#endif
