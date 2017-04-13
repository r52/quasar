#pragma once

#include <QMap>

enum QuasarSettingEntryType
{
    QUASAR_SETTING_ENTRY_INT = 0,
    QUASAR_SETTING_ENTRY_DOUBLE,
    QUASAR_SETTING_ENTRY_BOOL
};

struct quasar_setting_def_t
{
    QuasarSettingEntryType type;
    QString description;

    union
    {
        struct
        {
            int min;
            int max;
            int step;
            int def;
            int val;
        } inttype;

        struct
        {
            double min;
            double max;
            double step;
            double def;
            double val;
        } doubletype;

        struct
        {
            bool def;
            bool val;
        } booltype;
    };
};

struct quasar_settings_t
{
    QMap<QString, quasar_setting_def_t> map;
};
