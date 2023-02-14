/*! \file
    \brief C++ specific Extension API support functions

    \verbatim embed:rst

    .. attention::

        Functions in this file passes STL types across library boundaries.
        ENSURE THAT CRT LINKAGE IS SET TO DYNAMIC ON WINDOWS WHEN USING THESE!!!
    \endverbatim
*/

#pragma once

#include "extension_support.h"

#if defined(__cplusplus)

#  include <string>
#  include <vector>

//! Sets the return data to be a null terminated string
/*! \param[in]  hData   Data handle
    \param[in]  data    Data to set
    \return Data handle if successful, nullptr otherwise
*/
SAPI_EXPORT quasar_data_handle quasar_set_data_string_hpp(quasar_data_handle hData, std::string_view data);

//! Sets the return data to be a valid JSON object string
/*! \param[in]  hData   Data handle
    \param[in]  data    Data to set
    \return Data handle if successful, nullptr otherwise
*/
SAPI_EXPORT quasar_data_handle quasar_set_data_json_hpp(quasar_data_handle hData, std::string_view data);

//! Sets the return data to be an array of null terminated strings
/*! \param[in]  hData   Data handle
    \param[in]  vec     Vector of data to set
    \return Data handle if successful, nullptr otherwise
*/
SAPI_EXPORT quasar_data_handle quasar_set_data_string_vector(quasar_data_handle hData, const std::vector<std::string>& vec);

//! Sets the return data to be an array of integers
/*! \param[in]  hData   Data handle
    \param[in]  vec     Vector of data to set
    \return Data handle if successful, nullptr otherwise
*/
SAPI_EXPORT quasar_data_handle quasar_set_data_int_vector(quasar_data_handle hData, const std::vector<int>& vec);

//! Sets the return data to be an array of floats
/*! \param[in]  hData   Data handle
    \param[in]  vec     Vector of data to set
    \return Data handle if successful, nullptr otherwise
*/
SAPI_EXPORT quasar_data_handle quasar_set_data_float_vector(quasar_data_handle hData, const std::vector<float>& vec);

//! Sets the return data to be an array of doubles
/*! \param[in]  hData   Data handle
    \param[in]  vec     Vector of data to set
    \return Data handle if successful, nullptr otherwise
*/
SAPI_EXPORT quasar_data_handle quasar_set_data_double_vector(quasar_data_handle hData, const std::vector<double>& vec);

//! Retrieves a string setting from Quasar
/*!
    \param[in]  handle      Extension handle
    \param[in]  settings    The extension settings handle
    \param[in]  name        Name of the setting
    \return string setting if successful, empty string_view otherwise
*/
SAPI_EXPORT std::string_view quasar_get_string_setting_hpp(quasar_ext_handle handle, quasar_settings_t* settings, std::string_view name);

//! Retrieves a selection setting from Quasar
/*!
    \param[in]  handle      Extension handle
    \param[in]  settings    The extension settings handle
    \param[in]  name        Name of the setting
    \return string setting if successful, empty string_view otherwise
*/
SAPI_EXPORT std::string_view quasar_get_selection_setting_hpp(quasar_ext_handle handle, quasar_settings_t* settings, std::string_view name);

#endif
