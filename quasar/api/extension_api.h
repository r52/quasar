/*! \file
    \brief Base Extension functions

    This file defines base functions that must be implemented by a Quasar extension.
    These are called to load and destroy the extension.
*/

#pragma once

#include "extension_types.h"

#if defined(_WIN32)
#  define EXPORT __declspec(dllexport)
#else
#  define EXPORT __attribute__((visibility("default")))
#endif

#if defined(__cplusplus)
extern "C" {
#endif

//! Loads this extension
/*! This function should only populate a \ref quasar_ext_info_t struct with this extension's info
    and return it. Allocations for resources required by this extension should be performed in
    \ref quasar_ext_info_t.init instead.

    \sa quasar_ext_info_t, quasar_ext_info_t.init
    \return pointer to a populated \ref quasar_ext_info_t struct if successful, nullptr otherwise
*/
EXPORT quasar_ext_info_t* quasar_ext_load(void);

//! Destroys this extension
/*! This function should extension any resources allocated for the \ref quasar_ext_info_t struct
    (as well as the struct instance itself if necessary)

    \param[in]  info    Extension info data

    \sa quasar_ext_info_t
*/
EXPORT void quasar_ext_destroy(quasar_ext_info_t* info);

#if defined(__cplusplus)
}
#endif
