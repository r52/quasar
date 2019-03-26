/*! \file
    \brief Internal types for processing extension_support.h functions. Not a part of API.

    This file is **NOT** a part of the Extension API and is not shipped with the
    API package. For reference only. This file is subjected to major changes.
*/

#pragma once

#include <QVariant>
#include <qstring_hash_impl.h>
#include <unordered_map>

//! Enum for data types supported by extension settings definitions
enum QuasarSettingEntryType
{
    QUASAR_SETTING_ENTRY_INT = 0,  //!< Integer type
    QUASAR_SETTING_ENTRY_DOUBLE,   //!< Double type
    QUASAR_SETTING_ENTRY_BOOL,     //!< Bool type
    QUASAR_SETTING_ENTRY_SELECTION //!< Selection type
};

// Helper types

//! Internal struct holding an integer type setting
struct esi_inttype_t
{
    int min;  //!< Minimum value
    int max;  //!< Maximum value
    int step; //!< Increment steps (for GUI)
    int def;  //!< Default value
    int val;  //!< Current value
};

//! Internal struct holding a double type setting
struct esi_doubletype_t
{
    double min;  //!< Minimum value
    double max;  //!< Maximum value
    double step; //!< Increment steps (for GUI)
    double def;  //!< Default value
    double val;  //!< Current value
};

//! Internal struct holding a bool type setting
struct esi_booltype_t
{
    bool def; //!< Default value
    bool val; //!< Current value
};

//! Internal struct holding a selection type setting
struct esi_selecttype_t
{
    QStringList list; //!< Possible values
    QString     val;  //!< Current value
};

Q_DECLARE_METATYPE(esi_inttype_t);
Q_DECLARE_METATYPE(esi_doubletype_t);
Q_DECLARE_METATYPE(esi_booltype_t);
Q_DECLARE_METATYPE(esi_selecttype_t);

//! Struct for defining a extension setting entry
/*! \sa extension_support.h
 */
struct quasar_setting_def_t
{
    QuasarSettingEntryType type;        //!< The data type used for this setting entry. \sa QuasarSettingEntryType
    QString                description; //!< Description of this setting entry

    //! Unified storage for the setting definition
    /*! Depending on the value of \ref type, one of the structs above will be the content of this QVariant which
        contain the valid values for this definition.

        i.e.:
        type of QUASAR_SETTING_ENTRY_INT refers to esi_inttype_t
        type of QUASAR_SETTING_ENTRY_DOUBLE refers to esi_doubletype_t

        etc.

        \sa QuasarSettingEntryType
    */
    QVariant var;
};

//! Struct for creating and storing extension settings
struct quasar_settings_t
{
    //! Map of \ref quasar_setting_def_t custom settings
    std::unordered_map<QString, quasar_setting_def_t> map;
};
