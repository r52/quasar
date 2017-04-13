#pragma once

#include "plugin_types.h"

#if defined(__cplusplus)
extern "C" {
#endif

    EXPORT void quasar_log(quasar_log_level_t level, const char* msg);

    EXPORT quasar_settings_t* quasar_create_settings(void);

    EXPORT quasar_settings_t* quasar_add_int(quasar_settings_t* settings, const char *name, const char *description, int min, int max, int step);
    EXPORT quasar_settings_t* quasar_add_bool(quasar_settings_t* settings, const char *name, const char *description);
    EXPORT quasar_settings_t* quasar_add_double(quasar_settings_t* settings, const char *name, const char *description, double min, double max, double step);

    EXPORT intmax_t quasar_get_int(quasar_settings_t* settings, const char* name);
    EXPORT uintmax_t quasar_get_uint(quasar_settings_t* settings, const char* name);
    EXPORT bool quasar_get_bool(quasar_settings_t* settings, const char* name);
    EXPORT double quasar_get_double(quasar_settings_t* settings, const char* name);

#if defined(__cplusplus)
}
#endif
