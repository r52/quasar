#pragma once

#include "plugin_types.h"

#if defined(__cplusplus)
extern "C" {
#endif

// quasar_plugin_load
//
// returns plugin info data if successful, nullptr otherwise
EXPORT quasar_plugin_info_t* quasar_plugin_load(void);

#if defined(__cplusplus)
}
#endif
