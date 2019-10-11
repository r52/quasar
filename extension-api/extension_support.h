/*! \file
    \brief Extension API support functionality

    This file defines functions that augment extension functionality
*/

#pragma once

#include "extension_types.h"

#ifdef EXTENSIONAPI_LIB
#    ifdef _WIN32
#        define SAPI_EXPORT __declspec(dllexport)
#    else
#        define SAPI_EXPORT __attribute__((visibility("default")))
#    endif // _WIN32
#else
#    define SAPI_EXPORT
#endif // EXTENSIONAPI_LIB

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

//! Frees any dangling created instances. Should only be used in error case exits.
/*!
    \param[in]  handle  Handle to instance to be freed
*/
SAPI_EXPORT void quasar_free(void* handle);

//! Creates a new instance of \ref quasar_settings_t
/*! Use in \ref quasar_ext_info_t.create_settings to create extension settings.
    Ensure that only a single instance is created and used per extension.

    \return quasar_settings_t instance if successful, nullptr otherwise
*/
SAPI_EXPORT quasar_settings_t* quasar_create_settings(void);

//! Creates a new \ref quasar_selection_options_t setting.
/*! Use with \ref quasar_add_selection() after populating options.

    \return quasar_selection_options_t instance if successful, nullptr otherwise
*/
SAPI_EXPORT quasar_selection_options_t* quasar_create_selection_setting(void);

//! Sets the return data to be a null terminated string
/*! \param[in]  hData   Data handle
    \param[in]  data    Data to set
    \return Data handle if successful, nullptr otherwise
*/
SAPI_EXPORT quasar_data_handle quasar_set_data_string(quasar_data_handle hData, const char* data);

//! Sets the return data to be an integer
/*! \param[in]  hData   Data handle
    \param[in]  data    Data to set
    \return Data handle if successful, nullptr otherwise
*/
SAPI_EXPORT quasar_data_handle quasar_set_data_int(quasar_data_handle hData, int data);

//! Sets the return data to be a floating point double
/*! \param[in]  hData   Data handle
    \param[in]  data    Data to set
    \return Data handle if successful, nullptr otherwise
*/
SAPI_EXPORT quasar_data_handle quasar_set_data_double(quasar_data_handle hData, double data);

//! Sets the return data to be a bool
/*! \param[in]  hData   Data handle
    \param[in]  data    Data to set
    \return Data handle if successful, nullptr otherwise
*/
SAPI_EXPORT quasar_data_handle quasar_set_data_bool(quasar_data_handle hData, bool data);

//! Sets the return data to be a valid JSON object string
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

//! Sets the return data to be null
/*! \param[in]  hData   Data handle
    \return Data handle if successful, nullptr otherwise
*/
SAPI_EXPORT quasar_data_handle quasar_set_data_null(quasar_data_handle hData);

//! Adds an error to the return data to be sent back to the client
/*! \param[in]  hData   Data handle
    \param[in]  err     Error to add
    \return Data handle if successful, nullptr otherwise
*/
SAPI_EXPORT quasar_data_handle quasar_append_error(quasar_data_handle hData, const char* err);

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

//! Creates a string type setting in extension settings
/*!
    \param[in]  settings    The extension settings handle
    \param[in]  name        Name of the setting
    \param[in]  description Description for the setting
    \param[in]  dflt        Default value (if any)
    \param[in]  password    Whether this field is a password/obscured field in the UI
    \return The settings handle if successful, nullptr otherwise
*/
SAPI_EXPORT quasar_settings_t* quasar_add_string(quasar_settings_t* settings, const char* name, const char* description, const char* dflt, bool password);

//! Creates a selection type setting in extension settings
/*!
    \param[in]  settings    The extension settings handle
    \param[in]  name        Name of the setting
    \param[in]  description Description for the setting
    \param[in]  select      Handle to the selection setting instance (takes ownership)
    \return The settings handle if successful, nullptr otherwise
*/
SAPI_EXPORT quasar_settings_t* quasar_add_selection(quasar_settings_t* settings, const char* name, const char* description, quasar_selection_options_t* select);

//! Creates a selection type setting in extension settings
/*!
    \param[in]  select      The selection setting handle
    \param[in]  name        Name of the option (shown in UI)
    \param[in]  value       Actual value of the option
    \return The setting handle if successful, nullptr otherwise
*/
SAPI_EXPORT quasar_selection_options_t* quasar_add_selection_option(quasar_selection_options_t* select, const char* name, const char* value);

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

//! Retrieves a string setting from Quasar
/*!
    \param[in]  settings    The extension settings handle
    \param[in]  name        Name of the setting
    \param[in]  buf         Buffer to copy results to
    \param[in]  size        Size of buffer
    \return true if successful, false otherwise
*/
SAPI_EXPORT bool quasar_get_string(quasar_settings_t* settings, const char* name, char* buf, size_t size);

//! Retrieves a selection setting from Quasar
/*!
    \param[in]  settings    The extension settings handle
    \param[in]  name        Name of the setting
    \param[in]  buf         Buffer to copy results to
    \param[in]  size        Size of buffer
    \return true if successful, false otherwise
*/
SAPI_EXPORT bool quasar_get_selection(quasar_settings_t* settings, const char* name, char* buf, size_t size);

//! Signals to Quasar that data is ready to be sent to clients
/*! This function is for Data Sources with \ref quasar_data_source_t.rate
    set to \ref QUASAR_POLLING_CLIENT or \ref QUASAR_POLLING_SIGNALED.
    This function signals to Quasar that the data for the specified source is ready to be sent.

    \param[in]  handle  Extension handle
    \param[in]  source  Data Source identifier

    \sa quasar_data_source_t.rate
*/
SAPI_EXPORT void quasar_signal_data_ready(quasar_ext_handle handle, const char* source);

//! Waits for a set of data to be sent to clients before processing the next set
/*! This function is for Data Sources with \ref quasar_data_source_t.rate
    set to \ref QUASAR_POLLING_SIGNALED. This function can be used to allow a thread to wait until a set of data
    has been consumed before moving on to processing the next set.

    \param[in]  handle  Extension handle
    \param[in]  source  Data Source identifier

    \sa quasar_data_source_t.rate
*/
SAPI_EXPORT void quasar_signal_wait_processed(quasar_ext_handle handle, const char* source);

//! Stores a string type data
/*! \param[in]  handle  Extension handle
    \param[in]  name    Data name
    \param[in]  data    Data to set
*/
SAPI_EXPORT void quasar_set_storage_string(quasar_ext_handle handle, const char* name, const char* data);

//! Stores a int type data
/*! \param[in]  handle  Extension handle
    \param[in]  name    Data name
    \param[in]  data    Data to set
*/
SAPI_EXPORT void quasar_set_storage_int(quasar_ext_handle handle, const char* name, int data);

//! Stores a double type data
/*! \param[in]  handle  Extension handle
    \param[in]  name    Data name
    \param[in]  data    Data to set
*/
SAPI_EXPORT void quasar_set_storage_double(quasar_ext_handle handle, const char* name, double data);

//! Stores a bool type data
/*! \param[in]  handle  Extension handle
    \param[in]  name    Data name
    \param[in]  data    Data to set
*/
SAPI_EXPORT void quasar_set_storage_bool(quasar_ext_handle handle, const char* name, bool data);

//! Gets a string type data from storage
/*! \param[in]  handle  Extension handle
    \param[in]  name    Data name
    \param[in]  buf     Buffer to copy results to
    \param[in]  size    Size of buffer
    \return true if successful, false otherwise
*/
SAPI_EXPORT bool quasar_get_storage_string(quasar_ext_handle handle, const char* name, char* buf, size_t size);

//! Gets a int type data from storage
/*! \param[in]  handle  Extension handle
    \param[in]  name    Data name
    \param[in]  buf     Buffer to copy results to
    \return true if successful, false otherwise
*/
SAPI_EXPORT bool quasar_get_storage_int(quasar_ext_handle handle, const char* name, int* buf);

//! Gets a double type data from storage
/*! \param[in]  handle  Extension handle
    \param[in]  name    Data name
    \param[in]  buf     Buffer to copy results to
    \return true if successful, false otherwise
*/
SAPI_EXPORT bool quasar_get_storage_double(quasar_ext_handle handle, const char* name, double* buf);

//! Gets a bool type data from storage
/*! \param[in]  handle  Extension handle
    \param[in]  name    Data name
    \param[in]  buf     Buffer to copy results to
    \return true if successful, false otherwise
*/
SAPI_EXPORT bool quasar_get_storage_bool(quasar_ext_handle handle, const char* name, bool* buf);

#if defined(__cplusplus)
}
#endif
