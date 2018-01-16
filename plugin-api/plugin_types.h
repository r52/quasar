#pragma once

#if defined(__cplusplus)
#    include <cstddef>
#    include <cstdint>
#else
#    include <stdbool.h>
#    include <stddef.h>
#    include <stdint.h>
#endif

#define QUASAR_API_VERSION 1

#if defined(__cplusplus)
extern "C" {
#endif

enum quasar_log_level_t
{
    QUASAR_LOG_DEBUG,
    QUASAR_LOG_INFO,
    QUASAR_LOG_WARNING,
    QUASAR_LOG_CRITICAL
};

struct quasar_settings_t;

typedef void* quasar_plugin_handle;
typedef void* quasar_data_handle;

struct quasar_data_source_t
{
    char    dataSrc[32]; // codename of this data source
    int64_t refreshMsec; // default rate of refresh for this data source (0 = data is polled by the client; -1 = plugin responsible for signaling data send)
    size_t  uid;         // uid assigned to this data source by quasar
};

struct quasar_plugin_info_t
{
    typedef bool (*plugin_info_call_t)(quasar_plugin_handle);
    typedef quasar_settings_t* (*plugin_create_settings_call_t)();
    typedef void (*plugin_settings_call_t)(quasar_settings_t*);
    typedef bool (*plugin_get_data_call_t)(size_t, quasar_data_handle);

    // static info
    int  api_version;      // API version. Should always be initialized to QUASAR_API_VERSION
    char name[64];         // name of this plugin
    char code[16];         // unique short code for widgets to identify and subscribe to this plugin
    char version[64];      // version string
    char author[64];       // author
    char description[256]; // plugin description

    size_t                numDataSources; // number of data sources
    quasar_data_source_t* dataSources;    // list of data sources this plugin offers

    // calls

    // init(quasar_plugin_handle handle), required
    //
    // This function should save data source uids assigned by Quasar
    // as well as initialize anything else needed
    //
    // returns true if success, false otherwise
    plugin_info_call_t init;

    // shutdown(quasar_plugin_handle handle), required
    //
    // This function should cleanup anything initialized
    //
    // Should always return true
    plugin_info_call_t shutdown;

    // get_data(size_t uid, quasar_data_handle handle), required
    //
    // Retrieves the data of a specific data entry
    //
    // Use functions in plugin_support.h to populate data
    //
    // returns true if success, false otherwise
    //
    // This function needs to be re-entrant
    plugin_get_data_call_t get_data;

    // create_settings(), optional
    //
    // Creates settings UI if any
    //
    // Returns quasar_settings_t* struct pointer if created, nullptr otherwise
    plugin_create_settings_call_t create_settings;

    // update(quasar_settings_t* settings), optional
    //
    // This function should update local settings values
    plugin_settings_call_t update;
};

#if defined(__cplusplus)
}
#endif
