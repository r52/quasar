/*! \file
    \brief Types used by the Plugin API.

    This file defines types used by the Plugin API. Included by plugin_api.h.
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

//! Quasar Plugin API version.
#define QUASAR_API_VERSION 1

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

//! Struct for creating and storing plugin settings.
/*! This struct is opaque to the front facing API.
    \sa plugin_support.h, plugin_support_internal.h
*/
struct quasar_settings_t;

//! Type for the plugin handle pointer.
typedef void* quasar_plugin_handle;

//! Handle type for storing return data.
/*! \sa plugin_support.h, quasar_plugin_info_t.get_data */
typedef void* quasar_data_handle;

//! Struct for defining Data Sources.
/*! Defines the Data Sources available to widgets provided by this plugin.
    \sa quasar_plugin_info_t.dataSources
*/
struct quasar_data_source_t
{
    char dataSrc[32]; //!< Codename of a Data Source.

    int64_t refreshMsec; //!< Default rate of refresh for this Data Source (in milliseconds).
                         //!< 0 = data is polled on-demand by the client, -1 = plugin is responsible for signaling data send when data is ready.
                         //!< \sa quasar_signal_data_ready(), quasar_signal_wait_processed()

    size_t uid; //!< uid assigned to this Data Source by Quasar.
                //!< An integer uid is assigned to each Data Source by Quasar to reduce
                //!< discrepancies and avoid string comparisons. This uid is passed to
                //!< \ref quasar_plugin_info_t.get_data.
};

//! Struct for defining a Quasar Plugin.
/*! A plugin should populate this struct with data upon initialization
    and return it to Quasar when \ref quasar_plugin_load() is called
    \sa quasar_plugin_load()
*/
struct quasar_plugin_info_t
{
    //! Function pointer type for the \ref init and \ref shutdown functions.
    /*! \sa init, shutdown */
    typedef bool (*plugin_info_call_t)(quasar_plugin_handle);

    //! Function pointer type for the \ref create_settings function.
    /*! \sa create_settings */
    typedef quasar_settings_t* (*plugin_create_settings_call_t)();

    //! Function pointer type for settings related functions, like \ref update.
    /*! \sa update */
    typedef void (*plugin_settings_call_t)(quasar_settings_t*);

    //! Function pointer type for the \ref get_data function.
    /*! \sa get_data */
    typedef bool (*plugin_get_data_call_t)(size_t, quasar_data_handle);

    int  api_version;      //!< API version. Should always be initialized to #QUASAR_API_VERSION.
    char name[64];         //!< Name of this plugin.
    char code[16];         //!< A unique short code identifier for widgets to identify and subscribe to this plugin.
    char version[64];      //!< Version string.
    char author[64];       //!< Author.
    char description[256]; //!< Plugin description.

    size_t                numDataSources; //!< Number of Data Sources provided by this plugin.
    quasar_data_source_t* dataSources;    //!< Array of Data Sources provided by this plugin.
                                          //!< \sa quasar_data_source_t

    /*! **bool init(#quasar_plugin_handle handle)**
        \verbatim embed:rst

        .. attention::

            Plugins are **REQUIRED** to implement this function.
        \endverbatim

        This function should save data source uids assigned by Quasar
        as well as initialize any resources needed by the plugin.
        The plugin's handle will be passed into this function.
        This function should save the handle.

        \return true if success, false otherwise
    */
    plugin_info_call_t init;

    /*! **bool shutdown(#quasar_plugin_handle handle)**
        \verbatim embed:rst

        .. attention::

            Plugins are **REQUIRED** to implement this function.
        \endverbatim

        This function should cleanup any resources initialized by this plugin.

        \return Should always return true
    */
    plugin_info_call_t shutdown;

    /*! **bool get_data(size_t uid, #quasar_data_handle handle)**
        \verbatim embed:rst

        .. attention::

            Plugins are **REQUIRED** to implement this function.
        \endverbatim

        Retrieves the data of a specific Data Source entry.

        Use support functions in plugin_support.h to populate data.

        \verbatim embed:rst
        .. important::

            This function needs to be both re-entrant and thread-safe.
        \endverbatim

        \sa quasar_set_data_string(), quasar_set_data_json(), quasar_set_data_binary(),
            quasar_set_data_string_array(), quasar_set_data_int_array(), quasar_set_data_float_array(),
            quasar_set_data_double_array()
        \return true if success, false otherwise
    */
    plugin_get_data_call_t get_data;

    //! quasar_settings_t* create_settings(), **OPTIONAL**
    /*! Creates plugin settings (and corresponding UI elements) if any.

        \sa quasar_create_settings(), quasar_add_int(), quasar_add_bool(), quasar_add_double()
        \return quasar_settings_t pointer if successful, nullptr otherwise
    */
    plugin_create_settings_call_t create_settings;

    //! void update(quasar_settings_t* settings), **OPTIONAL**
    /*! This function should update local settings values.

        \sa quasar_get_int(), quasar_get_uint(), quasar_get_bool(), quasar_get_double()
    */
    plugin_settings_call_t update;
};

#if defined(__cplusplus)
}
#endif
