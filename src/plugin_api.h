#pragma once

#include "plugin_types.h"

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(WIN32)
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif

    // WIP

    /*

    JSON data payload (see plugin_types.h)

    {
        type: "data",
        plugin: <pluginCode>,
        source: <dataSrc>,
        data: <payload from quasar_plugin_get_data(dataSrc)>
    }

    */

    // Free plugin allocated data
    EXPORT void quasar_plugin_free(void* ptr, int size);

    // Plugin init
    //
    // returns true if successful, false otherwise
    EXPORT bool quasar_plugin_init(QuasarPluginInfo* info);

    // Get data
    //
    // Retrieves the data of a specific data entry
    // as a JSON-able string containing the requested data
    //
    // returns true if success, false otherwise
    EXPORT bool quasar_plugin_get_data(const char* dataSrc, char* buf, int bufsz);

#if defined(__cplusplus)
}
#endif
