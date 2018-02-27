/*! \file
    \brief Internal types for processing plugin_support.h functions. Not a part of API.

    This file is **NOT** a part of the Plugin API and is not shipped with the
    API package. For reference only. This file is subjected to major changes.
*/

#pragma once

#include <qstring_hash_impl.h>
#include <unordered_map>

//! Enum for data types supported by plugin settings definitions
enum QuasarSettingEntryType
{
    QUASAR_SETTING_ENTRY_INT = 0, //!< Integer type
    QUASAR_SETTING_ENTRY_DOUBLE,  //!< Double type
    QUASAR_SETTING_ENTRY_BOOL     //!< Bool type
};

//! Struct for defining a plugin setting entry
/*! \sa plugin_support.h
*/
struct quasar_setting_def_t
{
    QuasarSettingEntryType type;        //!< The data type used for this setting entry. \sa QuasarSettingEntryType
    QString                description; //!< Description of this setting entry

    //! Unified storage for the setting definition
    /*! Depending on the value of \ref type, one of the structs in this union will
        contain the valid values for this definition.

        i.e.:
        type of QUASAR_SETTING_ENTRY_INT refers to inttype
        type of QUASAR_SETTING_ENTRY_DOUBLE refers to doubletype

        etc.

        \sa QuasarSettingEntryType
    */
    union
    {
        struct
        {
            int min;  //!< Minimum value
            int max;  //!< Maximum value
            int step; //!< Increment steps (for GUI)
            int def;  //!< Default value
            int val;  //!< Current value
        } inttype;

        struct
        {
            double min;  //!< Minimum value
            double max;  //!< Maximum value
            double step; //!< Increment steps (for GUI)
            double def;  //!< Default value
            double val;  //!< Current value
        } doubletype;

        struct
        {
            bool def; //!< Default value
            bool val; //!< Current value
        } booltype;
    };
};

//! Struct for creating and storing plugin settings
struct quasar_settings_t
{
    //! Map of \ref quasar_setting_def_t custom settings
    std::unordered_map<QString, quasar_setting_def_t> map;
};
