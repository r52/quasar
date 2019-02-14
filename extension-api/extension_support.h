/*! \file
    \brief Extension API support functionality

    This file defines functions that augment extension functionality
*/

#pragma once

#include "extension_types.h"

#ifdef PLUGINAPI_LIB
#    ifdef _WIN32
#        define SAPI_EXPORT __declspec(dllexport)
#    else
#        define SAPI_EXPORT __attribute__((visibility("default")))
#    endif // _WIN32
#else
#    define SAPI_EXPORT
#endif // PLUGINAPI_LIB

#if defined(__cplusplus)
extern "C" {
#endif

//! Logs a message to Quasar console
/*!
    \param[in]  level   Log level
    \param[in]  msg     Log message
    \sa quasar_log_level_t
*/
SAPI_EXPORT void quasar_log(quasar_log_level_t level, const char* msg);

//! Creates a new instance \ref quasar_settings_t
/*! Use in \ref quasar_ext_info_t.create_settings to create extension settings.
    Ensure that only a single instance is created and used per extension.

    \return quasar_settings_t instance if successful, nullptr otherwise
*/
SAPI_EXPORT quasar_settings_t* quasar_create_settings(void);

//! Sets the return data to be a null terminated string
/*! \param[in]  hData   Data handle
    \param[in]  data    Data to set
    \return Data handle if successful, nullptr otherwise
*/
SAPI_EXPORT quasar_data_handle quasar_set_data_string(quasar_data_handle hData, const char* data);

//! Sets the return data to be a valid JSON string
/*! \param[in]  hData   Data handle
    \param[in]  data    Data to set
    \return Data handle if successful, nullptr otherwise
*/
SAPI_EXPORT quasar_data_handle quasar_set_data_json(quasar_data_handle hData, const char* data);

//! Sets the return data to be a binary payload
/*! \param[in]  hData   Data handle
    \param[in]  data    Data to set
    \param[in]  len     Data length
    \return Data handle if successful, nullptr otherwise
*/
SAPI_EXPORT quasar_data_handle quasar_set_data_binary(quasar_data_handle hData, const char* data, size_t len);

//! Sets the return data to be an array of null terminated strings
/*! \param[in]  hData   Data handle
    \param[in]  arr     Array of data to set
    \param[in]  len     Length of array
    \return Data handle if successful, nullptr otherwise
*/
SAPI_EXPORT quasar_data_handle quasar_set_data_string_array(quasar_data_handle hData, char** arr, size_t len);

//! Sets the return data to be an array of integers
/*! \param[in]  hData   Data handle
    \param[in]  arr     Array of data to set
    \param[in]  len     Length of array
    \return Data handle if successful, nullptr otherwise
*/
SAPI_EXPORT quasar_data_handle quasar_set_data_int_array(quasar_data_handle hData, int* arr, size_t len);

//! Sets the return data to be an array of floats
/*! \param[in]  hData   Data handle
    \param[in]  arr     Array of data to set
    \param[in]  len     Length of array
    \return Data handle if successful, nullptr otherwise
*/
SAPI_EXPORT quasar_data_handle quasar_set_data_float_array(quasar_data_handle hData, float* arr, size_t len);

//! Sets the return data to be an array of doubles
/*! \param[in]  hData   Data handle
    \param[in]  arr     Array of data to set
    \param[in]  len     Length of array
    \return Data handle if successful, nullptr otherwise
*/
SAPI_EXPORT quasar_data_handle quasar_set_data_double_array(quasar_data_handle hData, double* arr, size_t len);

//! Creates an integer setting in extension settings
/*!
    \param[in]  settings    The extension settings handle
    \param[in]  name        Name of the setting
    \param[in]  description Description for the setting
    \param[in]  min         Minimum value
    \param[in]  max         Maximum value
    \param[in]  step        Incremental step
    \param[in]  dflt        Default value
    \return The settings handle if successful, nullptr otherwise
*/
SAPI_EXPORT quasar_settings_t* quasar_add_int(quasar_settings_t* settings, const char* name, const char* description, int min, int max, int step, int dflt);

//! Creates a bool setting in extension settings
/*!
    \param[in]  settings    The extension settings handle
    \param[in]  name        Name of the setting
    \param[in]  description Description for the setting
    \param[in]  dflt        Default value
    \return The settings handle if successful, nullptr otherwise
*/
SAPI_EXPORT quasar_settings_t* quasar_add_bool(quasar_settings_t* settings, const char* name, const char* description, bool dflt);

//! Creates a double setting in extension settings
/*!
    \param[in]  settings    The extension settings handle
    \param[in]  name        Name of the setting
    \param[in]  description Description for the setting
    \param[in]  min         Minimum value
    \param[in]  max         Maximum value
    \param[in]  step        Incremental step
    \param[in]  dflt        Default value
    \return The settings handle if successful, nullptr otherwise
*/
SAPI_EXPORT quasar_settings_t*
            quasar_add_double(quasar_settings_t* settings, const char* name, const char* description, double min, double max, double step, double dflt);

//! Retrieves an integer setting from Quasar
/*!
    \param[in]  settings    The extension settings handle
    \param[in]  name        Name of the setting
    \return Value of the setting if successful, default value otherwise
*/
SAPI_EXPORT intmax_t quasar_get_int(quasar_settings_t* settings, const char* name);

//! Retrieves an unsigned integer setting from Quasar
/*!
    \param[in]  settings    The extension settings handle
    \param[in]  name        Name of the setting
    \return Value of the setting if successful, default value otherwise
*/
SAPI_EXPORT uintmax_t quasar_get_uint(quasar_settings_t* settings, const char* name);

//! Retrieves a bool setting from Quasar
/*!
    \param[in]  settings    The extension settings handle
    \param[in]  name        Name of the setting
    \return Value of the setting if successful, default value otherwise
*/
SAPI_EXPORT bool quasar_get_bool(quasar_settings_t* settings, const char* name);

//! Retrieves a double setting from Quasar
/*!
    \param[in]  settings    The extension settings handle
    \param[in]  name        Name of the setting
    \return Value of the setting if successful, default value otherwise
*/
SAPI_EXPORT double quasar_get_double(quasar_settings_t* settings, const char* name);

//! Signals to Quasar that data is ready to be sent to clients
/*! This function is for Data Sources with \ref quasar_data_source_t.refreshMsec
    set to 0 or -1. This function signals to Quasar that the data for the specified
    source is ready to be sent.

    \param[in]  handle  Extension handle
    \param[in]  source  Data Source codename

    \sa quasar_data_source_t.refreshMsec
*/
SAPI_EXPORT void quasar_signal_data_ready(quasar_ext_handle handle, const char* source);

//! Waits for a set of data to be sent to clients before processing the next set
/*! This function is for Data Sources with \ref quasar_data_source_t.refreshMsec
    set to -1. This function can be used to allow a thread to wait until a set of data
    has been consumed before moving on to processing the next set.

    \param[in]  handle  Extension handle
    \param[in]  source  Data Source codename

    \sa quasar_data_source_t.refreshMsec
*/
SAPI_EXPORT void quasar_signal_wait_processed(quasar_ext_handle handle, const char* source);

#if defined(__cplusplus)
}
#endif
