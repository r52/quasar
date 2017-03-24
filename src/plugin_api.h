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
    // See wiki for API definition

    // Free plugin allocated data
    EXPORT void quasar_plugin_free(void* ptr, int size);

    // Plugin init
    //
    // returns true if successful, false otherwise
    EXPORT int quasar_plugin_init(QuasarPluginInfo* info);

    // Get data
    //
    // Retrieves the data of a specific data entry
    // as a JSON-able string containing the requested data
    //
    // convert_data_to_json specifies whether this piece of data should be
    // treated as a JSON object (default false, meaning string)
    //
    // returns true if success, false otherwise
    EXPORT int quasar_plugin_get_data(const char* dataSrc, char* buf, int bufsz, int* convert_data_to_json);

#if defined(__cplusplus)
}
#endif
