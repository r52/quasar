#include "plugin_support.h"
#include "plugin_support_internal.h"

#include "dataplugin.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

void quasar_log(quasar_log_level_t level, const char* msg)
{
    QString lg = QString::fromUtf8(msg);
    switch (level)
    {
        case QUASAR_LOG_DEBUG:
            qDebug() << lg;
            break;
        case QUASAR_LOG_INFO:
        default:
            qInfo() << lg;
            break;
        case QUASAR_LOG_WARNING:
            qWarning() << lg;
            break;
        case QUASAR_LOG_CRITICAL:
            qCritical() << lg;
            break;
    }
}

quasar_settings_t* quasar_create_settings(void)
{
    return new quasar_settings_t;
}

quasar_data_handle quasar_set_data_string(quasar_data_handle hData, const char* data)
{
    QJsonValueRef* ref = (QJsonValueRef*) hData;

    if (ref)
    {
        (*ref) = QString::fromUtf8(data);

        return ref;
    }

    return nullptr;
}

quasar_data_handle quasar_set_data_json(quasar_data_handle hData, const char* data)
{
    QJsonValueRef* ref = (QJsonValueRef*) hData;

    if (ref)
    {
        QString str = QString::fromUtf8(data);
        (*ref)      = QJsonDocument::fromJson(str.toUtf8()).object();

        return ref;
    }

    return nullptr;
}

quasar_data_handle quasar_set_data_binary(quasar_data_handle hData, const char* data, size_t len)
{
    QJsonValueRef* ref = (QJsonValueRef*) hData;

    if (ref)
    {
        (*ref) = QJsonDocument::fromRawData(data, len).object();

        return ref;
    }

    return nullptr;
}

quasar_data_handle quasar_set_data_string_array(quasar_data_handle hData, char** arr, size_t len)
{
    QJsonValueRef* ref = (QJsonValueRef*) hData;

    if (ref)
    {
        QJsonArray jarr;

        for (size_t i = 0; i < len; i++)
        {
            jarr.append(QString(arr[i]));
        }

        (*ref) = jarr;

        return ref;
    }

    return nullptr;
}

quasar_data_handle quasar_set_data_int_array(quasar_data_handle hData, int* arr, size_t len)
{
    QJsonValueRef* ref = (QJsonValueRef*) hData;

    if (ref)
    {
        QJsonArray jarr;

        for (size_t i = 0; i < len; i++)
        {
            jarr.append(arr[i]);
        }

        (*ref) = jarr;

        return ref;
    }

    return nullptr;
}

quasar_data_handle quasar_set_data_float_array(quasar_data_handle hData, float* arr, size_t len)
{
    QJsonValueRef* ref = (QJsonValueRef*) hData;

    if (ref)
    {
        QJsonArray jarr;

        for (size_t i = 0; i < len; i++)
        {
            jarr.append((double) arr[i]);
        }

        (*ref) = jarr;

        return ref;
    }

    return nullptr;
}

quasar_data_handle quasar_set_data_double_array(quasar_data_handle hData, double* arr, size_t len)
{
    QJsonValueRef* ref = (QJsonValueRef*) hData;

    if (ref)
    {
        QJsonArray jarr;

        for (size_t i = 0; i < len; i++)
        {
            jarr.append(arr[i]);
        }

        (*ref) = jarr;

        return ref;
    }

    return nullptr;
}

quasar_settings_t* quasar_add_int(quasar_settings_t* settings, const char* name, const char* description, int min, int max, int step, int dflt)
{
    if (settings)
    {
        quasar_setting_def_t entry;

        entry.type         = QUASAR_SETTING_ENTRY_INT;
        entry.description  = description;
        entry.inttype.min  = min;
        entry.inttype.max  = max;
        entry.inttype.step = step;
        entry.inttype.def = entry.inttype.val = dflt;

        settings->map.insert(std::make_pair(name, entry));

        return settings;
    }

    return nullptr;
}

quasar_settings_t* quasar_add_bool(quasar_settings_t* settings, const char* name, const char* description, bool dflt)
{
    if (settings)
    {
        quasar_setting_def_t entry;

        entry.type         = QUASAR_SETTING_ENTRY_BOOL;
        entry.description  = description;
        entry.booltype.def = entry.booltype.val = dflt;

        settings->map.insert(std::make_pair(name, entry));

        return settings;
    }

    return nullptr;
}

quasar_settings_t* quasar_add_double(quasar_settings_t* settings, const char* name, const char* description, double min, double max, double step, double dflt)
{
    if (settings)
    {
        quasar_setting_def_t entry;

        entry.type            = QUASAR_SETTING_ENTRY_DOUBLE;
        entry.description     = description;
        entry.doubletype.min  = min;
        entry.doubletype.max  = max;
        entry.doubletype.step = step;
        entry.doubletype.def = entry.doubletype.val = dflt;

        settings->map.insert(std::make_pair(name, entry));

        return settings;
    }

    return nullptr;
}

intmax_t quasar_get_int(quasar_settings_t* settings, const char* name)
{
    if (settings && settings->map.count(name))
    {
        return settings->map[name].inttype.val;
    }

    return intmax_t();
}

uintmax_t quasar_get_uint(quasar_settings_t* settings, const char* name)
{
    if (settings && settings->map.count(name))
    {
        return settings->map[name].inttype.val;
    }

    return uintmax_t();
}

bool quasar_get_bool(quasar_settings_t* settings, const char* name)
{
    if (settings && settings->map.count(name))
    {
        return settings->map[name].booltype.val;
    }

    return false;
}

double quasar_get_double(quasar_settings_t* settings, const char* name)
{
    if (settings && settings->map.count(name))
    {
        return settings->map[name].doubletype.val;
    }

    return 0.0;
}

void quasar_signal_data_ready(quasar_plugin_handle handle, const char* source)
{
    DataPlugin* plugin = (DataPlugin*) handle;

    if (plugin)
    {
        plugin->emitDataReady(source);
    }
}

void quasar_signal_wait_processed(quasar_plugin_handle handle, const char* source)
{
    DataPlugin* plugin = (DataPlugin*) handle;

    if (plugin)
    {
        plugin->waitDataProcessed(source);
    }
}
