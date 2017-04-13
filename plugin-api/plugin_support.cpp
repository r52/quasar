#include "plugin_support.h"

#include <QDebug>

void quasar_log(quasar_log_level_t level, const char * msg)
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

// TODO: plugin settings support
quasar_settings_t * quasar_create_settings(void)
{
    return nullptr;
}

quasar_settings_t * quasar_add_int(quasar_settings_t * settings, const char * name, const char * description, int min, int max, int step)
{
    return nullptr;
}

quasar_settings_t * quasar_add_bool(quasar_settings_t * settings, const char * name, const char * description)
{
    return nullptr;
}

quasar_settings_t * quasar_add_double(quasar_settings_t * settings, const char * name, const char * description, double min, double max, double step)
{
    return nullptr;
}

intmax_t quasar_get_int(quasar_settings_t * settings, const char * name)
{
    return intmax_t();
}

uintmax_t quasar_get_uint(quasar_settings_t * settings, const char * name)
{
    return uintmax_t();
}

bool quasar_get_bool(quasar_settings_t * settings, const char * name)
{
    return false;
}

double quasar_get_double(quasar_settings_t * settings, const char * name)
{
    return 0.0;
}

