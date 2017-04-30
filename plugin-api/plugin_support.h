#pragma once

#include "plugin_types.h"

#ifdef PLUGINAPI_LIB
#define SAPI_EXPORT __declspec(dllexport)
#else
#define SAPI_EXPORT __declspec(dllimport)
#endif // PLUGINAPI_LIB

#if defined(__cplusplus)
extern "C" {
#endif

SAPI_EXPORT void quasar_log(quasar_log_level_t level, const char* msg);

SAPI_EXPORT quasar_settings_t* quasar_create_settings(void);

SAPI_EXPORT quasar_settings_t* quasar_add_int(quasar_settings_t* settings, const char* name, const char* description, int min, int max, int step, int default);
SAPI_EXPORT quasar_settings_t* quasar_add_bool(quasar_settings_t* settings, const char* name, const char* description, bool default);
SAPI_EXPORT quasar_settings_t* quasar_add_double(quasar_settings_t* settings, const char* name, const char* description, double min, double max, double step, double default);

SAPI_EXPORT intmax_t quasar_get_int(quasar_settings_t* settings, const char* name);
SAPI_EXPORT uintmax_t quasar_get_uint(quasar_settings_t* settings, const char* name);
SAPI_EXPORT bool      quasar_get_bool(quasar_settings_t* settings, const char* name);
SAPI_EXPORT double    quasar_get_double(quasar_settings_t* settings, const char* name);

SAPI_EXPORT void quasar_signal_data_ready(quasar_plugin_handle handle, const char* source);
SAPI_EXPORT void quasar_signal_wait_processed(quasar_plugin_handle handle, const char* source);

#if defined(__cplusplus)
}
#endif
