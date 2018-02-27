/*! \file
    \brief Base Plugin functions

    This file defines base functions that must be implemented by a Quasar plugin.
    These are called to load and destroy the plugin.
*/

#pragma once

#include "plugin_types.h"

#if defined(_WIN32)
#    define EXPORT __declspec(dllexport)
#else
#    define EXPORT __attribute__((visibility("default")))
#endif

#if defined(__cplusplus)
extern "C" {
#endif

//! Loads this plugin
/*! This function should only populate a \ref quasar_plugin_info_t struct with this plugin's info
    and return it. Allocations for resources required by this plugin should be performed in
    \ref quasar_plugin_info_t.init instead.

    \sa quasar_plugin_info_t, quasar_plugin_info_t.init
    \return pointer to a populated \ref quasar_plugin_info_t struct if successful, nullptr otherwise
*/
EXPORT quasar_plugin_info_t* quasar_plugin_load(void);

//! Destroys this plugin
/*! This function should destroy any resources allocated for the \ref quasar_plugin_info_t struct
    (as well as the struct instance itself if necessary)

    \param[in]  info    Plugin info data

    \sa quasar_plugin_info_t
*/
EXPORT void quasar_plugin_destroy(quasar_plugin_info_t* info);

#if defined(__cplusplus)
}
#endif
