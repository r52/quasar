#include "plugin_support.h"

#include "plugin_support_internal.h"

#include <QDebug>

void quasar_log(quasar_log_level_t level, const char* msg)
{
    switch (level)
    {
        case QUASAR_LOG_DEBUG:
            qDebug() << msg;
            break;
        case QUASAR_LOG_INFO:
        default:
            qInfo() << msg;
            break;
        case QUASAR_LOG_WARNING:
            qWarning() << msg;
            break;
        case QUASAR_LOG_CRITICAL:
            qCritical() << msg;
            break;
    }
}

quasar_settings_t* quasar_create_settings(void)
{
    return new quasar_settings_t;
}

quasar_settings_t* quasar_add_int(quasar_settings_t* settings, const char* name, const char* description, int min, int max, int step, int default)
{
    if (settings)
    {
        quasar_setting_def_t entry;

        entry.type         = QUASAR_SETTING_ENTRY_INT;
        entry.description  = description;
        entry.inttype.min  = min;
        entry.inttype.max  = max;
        entry.inttype.step = step;
        entry.inttype.def = entry.inttype.val = default;

        settings->map.insert(name, entry);

        return settings;
    }

    return nullptr;
}

quasar_settings_t* quasar_add_bool(quasar_settings_t* settings, const char* name, const char* description, bool default)
{
    if (settings)
    {
        quasar_setting_def_t entry;

        entry.type         = QUASAR_SETTING_ENTRY_BOOL;
        entry.description  = description;
        entry.booltype.def = entry.booltype.val = default;

        settings->map.insert(name, entry);

        return settings;
    }

    return nullptr;
}

quasar_settings_t* quasar_add_double(quasar_settings_t* settings, const char* name, const char* description, double min, double max, double step, double default)
{
    if (settings)
    {
        quasar_setting_def_t entry;

        entry.type            = QUASAR_SETTING_ENTRY_DOUBLE;
        entry.description     = description;
        entry.doubletype.min  = min;
        entry.doubletype.max  = max;
        entry.doubletype.step = step;
        entry.doubletype.def = entry.doubletype.val = default;

        settings->map.insert(name, entry);

        return settings;
    }

    return nullptr;
}

intmax_t quasar_get_int(quasar_settings_t* settings, const char* name)
{
    if (settings && settings->map.contains(name))
    {
        return settings->map[name].inttype.val;
    }

    return intmax_t();
}

uintmax_t quasar_get_uint(quasar_settings_t* settings, const char* name)
{
    if (settings && settings->map.contains(name))
    {
        return settings->map[name].inttype.val;
    }

    return uintmax_t();
}

bool quasar_get_bool(quasar_settings_t* settings, const char* name)
{
    if (settings && settings->map.contains(name))
    {
        return settings->map[name].booltype.val;
    }

    return false;
}

double quasar_get_double(quasar_settings_t* settings, const char* name)
{
    if (settings && settings->map.contains(name))
    {
        return settings->map[name].doubletype.val;
    }

    return 0.0;
}
