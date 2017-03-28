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

    // Free plugin allocated data
    EXPORT void quasar_plugin_free(void* ptr, int size);

    // Plugin command
    //
    // Processes one of the commands
    //
    // returns true if successful, false otherwise
    EXPORT int quasar_plugin_init(int cmd, QuasarPluginInfo* info);

    // Get data
    //
    // Retrieves the data of a specific data entry
    //
    // treatDataType specifies the type this piece of data should be
    // treated as (default string)
    //
    // returns true if success, false otherwise
    //
    // This function needs to be re-entrant
    EXPORT int quasar_plugin_get_data(unsigned int srcUid, char* buf, int bufsz, int* treatDataType);

#if defined(__cplusplus)
}
#endif
