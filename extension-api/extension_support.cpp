#include "extension_support.h"
#include "extension_support_internal.h"

#include "dataextension.h"

#include <type_traits>

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
    QJsonValue* ref = static_cast<QJsonValue*>(hData);

    if (ref)
    {
        (*ref) = QString::fromUtf8(data);

        return ref;
    }

    return nullptr;
}

template <typename T, typename = std::enable_if_t<std::is_same_v<double, T> || std::is_same_v<int, T> || std::is_same_v<bool, T>, T>>
quasar_data_handle set_basic_json_type(quasar_data_handle hData, T data)
{
    QJsonValue* ref = static_cast<QJsonValue*>(hData);

    if (ref)
    {
        (*ref) = data;

        return ref;
    }

    return nullptr;
}

quasar_data_handle quasar_set_data_int(quasar_data_handle hData, int data)
{
    return set_basic_json_type(hData, data);
}

quasar_data_handle quasar_set_data_double(quasar_data_handle hData, double data)
{
    return set_basic_json_type(hData, data);
}

quasar_data_handle quasar_set_data_bool(quasar_data_handle hData, bool data)
{
    return set_basic_json_type(hData, data);
}

quasar_data_handle quasar_set_data_json(quasar_data_handle hData, const char* data)
{
    QJsonValue* ref = static_cast<QJsonValue*>(hData);

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
    QJsonValue* ref = static_cast<QJsonValue*>(hData);

    if (ref)
    {
        (*ref) = QJsonDocument::fromRawData(data, len).object();

        return ref;
    }

    return nullptr;
}

quasar_data_handle quasar_set_data_string_array(quasar_data_handle hData, const char** arr, size_t len)
{
    QJsonValue* ref = static_cast<QJsonValue*>(hData);

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
    QJsonValue* ref = static_cast<QJsonValue*>(hData);

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
    QJsonValue* ref = static_cast<QJsonValue*>(hData);

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
    QJsonValue* ref = static_cast<QJsonValue*>(hData);

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

        entry.type        = QUASAR_SETTING_ENTRY_INT;
        entry.description = description;

        esi_inttype_t c;
        c.min  = min;
        c.max  = max;
        c.step = step;
        c.def = c.val = dflt;

        entry.var.setValue(c);

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

        entry.type        = QUASAR_SETTING_ENTRY_BOOL;
        entry.description = description;

        esi_booltype_t c;
        c.def = c.val = dflt;

        entry.var.setValue(c);

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

        entry.type        = QUASAR_SETTING_ENTRY_DOUBLE;
        entry.description = description;

        esi_doubletype_t c;
        c.min  = min;
        c.max  = max;
        c.step = step;
        c.def = c.val = dflt;

        entry.var.setValue(c);

        settings->map.insert(std::make_pair(name, entry));

        return settings;
    }

    return nullptr;
}

quasar_settings_t* quasar_add_string(quasar_settings_t* settings, const char* name, const char* description, const char* dflt)
{
    if (settings)
    {
        quasar_setting_def_t entry;

        entry.type        = QUASAR_SETTING_ENTRY_STRING;
        entry.description = description;

        esi_stringtype_t c;

        c.def = dflt;

        entry.var.setValue(c);

        settings->map.insert(std::make_pair(name, entry));

        return settings;
    }

    return nullptr;
}

quasar_settings_t* quasar_add_selection(quasar_settings_t* settings, const char* name, const char* description, const char** vals, size_t len)
{
    if (settings)
    {
        quasar_setting_def_t entry;

        entry.type        = QUASAR_SETTING_ENTRY_SELECTION;
        entry.description = description;

        esi_selecttype_t c;

        for (size_t i = 0; i < len; i++)
        {
            c.list.append(vals[i]);
        }

        entry.var.setValue(c);

        settings->map.insert(std::make_pair(name, entry));

        return settings;
    }

    return nullptr;
}

intmax_t quasar_get_int(quasar_settings_t* settings, const char* name)
{
    if (settings && settings->map.count(name))
    {
        auto c = settings->map[name].var.value<esi_inttype_t>();
        return c.val;
    }

    return intmax_t();
}

uintmax_t quasar_get_uint(quasar_settings_t* settings, const char* name)
{
    if (settings && settings->map.count(name))
    {
        auto c = settings->map[name].var.value<esi_inttype_t>();
        return c.val;
    }

    return uintmax_t();
}

bool quasar_get_bool(quasar_settings_t* settings, const char* name)
{
    if (settings && settings->map.count(name))
    {
        auto c = settings->map[name].var.value<esi_booltype_t>();
        return c.val;
    }

    return false;
}

double quasar_get_double(quasar_settings_t* settings, const char* name)
{
    if (settings && settings->map.count(name))
    {
        auto c = settings->map[name].var.value<esi_doubletype_t>();
        return c.val;
    }

    return 0.0;
}

const char* quasar_get_string(quasar_settings_t* settings, const char* name)
{
    if (settings && settings->map.count(name))
    {
        auto c  = settings->map[name].var.value<esi_stringtype_t>();
        auto ba = c.val.toLocal8Bit();
        return ba.data();
    }

    return nullptr;
}

const char* quasar_get_selection(quasar_settings_t* settings, const char* name)
{
    if (settings && settings->map.count(name))
    {
        auto c  = settings->map[name].var.value<esi_selecttype_t>();
        auto ba = c.val.toLocal8Bit();
        return ba.data();
    }

    return nullptr;
}

void quasar_signal_data_ready(quasar_ext_handle handle, const char* source)
{
    DataExtension* ext = static_cast<DataExtension*>(handle);

    if (ext)
    {
        ext->emitDataReady(source);
    }
}

void quasar_signal_wait_processed(quasar_ext_handle handle, const char* source)
{
    DataExtension* ext = static_cast<DataExtension*>(handle);

    if (ext)
    {
        ext->waitDataProcessed(source);
    }
}
