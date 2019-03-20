/*! \file
    \brief Types used by the Extension API.

    This file defines types used by the Extension API. Included by extension_api.h.
*/

#pragma once

#if defined(__cplusplus)
#    include <cstddef>
#    include <cstdint>
#else
#    include <stdbool.h>
#    include <stddef.h>
#    include <stdint.h>
#endif

//! Quasar extension API version.
#define QUASAR_API_VERSION 2

#if defined(__cplusplus)
extern "C" {
#endif

//! Defines valid log levels for logging.
enum quasar_log_level_t
{
    QUASAR_LOG_DEBUG,   //!< Debug level.
    QUASAR_LOG_INFO,    //!< Info level.
    QUASAR_LOG_WARNING, //!< Warning level.
    QUASAR_LOG_CRITICAL //!< Critical level.
};

//! Defines valid polling type values.
/*! Positive values determine extension timed data refresh rate
    \sa quasar_data_source_t.rate
*/
enum quasar_polling_type_t
{
    QUASAR_POLLING_SIGNALED = -1, //!< Extension is responsible for signaling data send when data is ready.
    QUASAR_POLLING_CLIENT   = 0   //!< Data is polled on-demand by the client
};

//! Struct for creating and storing extension settings.
/*! This struct is opaque to the front facing API.
    \sa extension_support.h, extension_support_internal.h
*/
struct quasar_settings_t;

// Typedefs

//! Type for the extension handle pointer.
typedef void* quasar_ext_handle;

//! Handle type for storing return data.
/*! \sa extension_support.h, quasar_ext_info_t.get_data */
typedef void* quasar_data_handle;

//! Function pointer type for the \ref quasar_ext_info_t.init and \ref quasar_ext_info_t.shutdown functions.
/*! \sa quasar_ext_info_t.init, quasar_ext_info_t.shutdown */
typedef bool (*ext_info_call_t)(quasar_ext_handle);

//! Function pointer type for the \ref quasar_ext_info_t.create_settings function.
/*! \sa quasar_ext_info_t.create_settings */
typedef quasar_settings_t* (*ext_create_settings_call_t)();

//! Function pointer type for settings related functions, like \ref quasar_ext_info_t.update.
/*! \sa quasar_ext_info_t.update */
typedef void (*ext_settings_call_t)(quasar_settings_t*);

//! Function pointer type for the \ref quasar_ext_info_t.get_data function.
/*! \sa quasar_ext_info_t.get_data */
typedef bool (*ext_get_data_call_t)(size_t, quasar_data_handle);

//! Struct for defining Data Sources.
/*! Defines the Data Sources available to widgets provided by this extension.
    \sa quasar_ext_info_t.dataSources
*/
struct quasar_data_source_t
{
    char name[32]; //!< Identifier for this data source.

    int64_t rate; //!< Default rate of refresh for this Data Source (in milliseconds).
                  //!< See \ref quasar_polling_type_t for additional polling options.
                  //!< \sa quasar_signal_data_ready(), quasar_signal_wait_processed(), quasar_polling_type_t

    uint64_t validtime; //!< For client polled data (\ref QUASAR_POLLING_CLIENT), this defines the duration in milliseconds
                        //!< that newly retrieved data is cached remains valid. Additional poll requests during this valid duration
                        //!< will return the cached data. A value of 0 means the data is never cached. Not used for other polling types.
                        //!< \sa quasar_polling_type_t

    size_t uid; //!< uid assigned to this Data Source by Quasar.
                //!< An integer uid is assigned to each Data Source by Quasar to reduce
                //!< discrepancies and avoid string comparisons. This uid is passed to
                //!< \ref quasar_ext_info_t.get_data.
};

//! Struct for defining information and description fields for the extension.
/*! Defines the information and description fields for this extension.
    \sa quasar_ext_info_t.fields
*/
struct quasar_ext_info_fields_t
{
    char name[16];         //!< A unique short identifier for this extension. Used by widgets to identify and subscribe to this extension.
    char fullname[64];     //!< Full name of this extension.
    char version[64];      //!< Version string.
    char author[64];       //!< Author.
    char description[256]; //!< Extension description.
    char url[256];         //!< Extension website url, if any.
};

//! Struct for defining a Quasar extension.
/*! An extension should populate this struct with data upon initialization
    and return it to Quasar when \ref quasar_ext_load() is called
    \sa quasar_ext_load()
*/
struct quasar_ext_info_t
{
    int                       api_version; //!< API version. Should always be initialized to #QUASAR_API_VERSION.
    quasar_ext_info_fields_t* fields;      //!< Extension info/description fields. Must be initialized.

    size_t                numDataSources; //!< Number of Data Sources provided by this extension.
    quasar_data_source_t* dataSources;    //!< Array of Data Sources provided by this extension.
                                          //!< \sa quasar_data_source_t

    /*! **bool init(#quasar_ext_handle handle)**
        \verbatim embed:rst

        .. attention::

            Extensions are **REQUIRED** to implement this function.
        \endverbatim

        This function should save data source uids assigned by Quasar
        as well as initialize any resources needed by the extension.
        The extension's handle will be passed into this function.
        This function should save the handle.

        \return true if success, false otherwise
    */
    ext_info_call_t init;

    /*! **bool shutdown(#quasar_ext_handle handle)**
        \verbatim embed:rst

        .. attention::

            Extensions are **REQUIRED** to implement this function.
        \endverbatim

        This function should cleanup any resources initialized by this extension.

        \return Should always return true
    */
    ext_info_call_t shutdown;

    /*! **bool get_data(size_t uid, #quasar_data_handle handle)**
        \verbatim embed:rst

        .. attention::

           Extensions are **REQUIRED** to implement this function.
        \endverbatim

        Retrieves the data of a specific Data Source entry.

        Use support functions in extension_support.h to populate data.

        \verbatim embed:rst
        .. important::

            This function needs to be both re-entrant and thread-safe.
        \endverbatim

        \sa quasar_set_data_string(), quasar_set_data_json(), quasar_set_data_binary(),
            quasar_set_data_string_array(), quasar_set_data_int_array(), quasar_set_data_float_array(),
            quasar_set_data_double_array()
        \return true if success, false otherwise
    */
    ext_get_data_call_t get_data;

    //! quasar_settings_t* create_settings(), **OPTIONAL**
    /*! Creates extension settings (and corresponding UI elements) if any.

        \sa quasar_create_settings(), quasar_add_int(), quasar_add_bool(), quasar_add_double()
        \return quasar_settings_t pointer if successful, nullptr otherwise
    */
    ext_create_settings_call_t create_settings;

    //! void update(quasar_settings_t* settings), **OPTIONAL**
    /*! This function should update local settings values.

        \sa quasar_get_int(), quasar_get_uint(), quasar_get_bool(), quasar_get_double()
    */
    ext_settings_call_t update;
};

#if defined(__cplusplus)
}
#endif
