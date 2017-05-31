#pragma once

#include "plugin_types.h"

#if defined(_WIN32)
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

#if defined(__cplusplus)
extern "C" {
#endif

// quasar_plugin_load
//
// returns plugin info data if successful, nullptr otherwise
EXPORT quasar_plugin_info_t* quasar_plugin_load(void);

// quasar_plugin_destroy
//
// destroys plugin info data (at shutdown)
EXPORT void quasar_plugin_destroy(quasar_plugin_info_t* info);

#if defined(__cplusplus)
}
#endif
