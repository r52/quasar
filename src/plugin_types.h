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

    // Log function fp
    typedef void(*log_func_type)(int level, const char* msg);

    struct QuasarPluginDataSource
    {
        char dataSrc[32];           // codename of this data source
        unsigned int refreshMsec;   // rate of refresh for this data source (0 means never; populated once and never changed)
    };

    struct QuasarPluginInfo
    {
        log_func_type logFunc;
        char pluginName[64];                    // name of this plugin
        char pluginCode[16];                    // unique short code for widgets to identify and subscribe to this plugin
        char author[64];                        // author
        char description[256];                  // plugin description
        unsigned int numDataSources;            // number of data sources
        QuasarPluginDataSource* dataSources;    // list of data sources this plugin offers
    };

#if defined(__cplusplus)
}
#endif
