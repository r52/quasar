#include <algorithm>

#include "api/extension_support.h"
#include "extension_support_internal.h"

#include "extension.h"

#include "extension_support.h"
#include <spdlog/spdlog.h>

void quasar_log(quasar_log_level_t level, const char* msg)
{
    switch (level)
    {
        case QUASAR_LOG_DEBUG:
            SPDLOG_DEBUG(msg);
            break;
        case QUASAR_LOG_INFO:
        default:
            SPDLOG_INFO(msg);
            break;
        case QUASAR_LOG_WARNING:
            SPDLOG_WARN(msg);
            break;
        case QUASAR_LOG_ERROR:
            SPDLOG_ERROR(msg);
            break;
        case QUASAR_LOG_CRITICAL:
            SPDLOG_CRITICAL(msg);
            break;
    }
}

void quasar_free(void* handle)
{
    if (nullptr != handle)
    {
        delete handle;
    }
}

quasar_settings_t* quasar_create_settings(quasar_ext_handle handle)
{
    Extension* ext = static_cast<Extension*>(handle);

    if (ext)
    {
        return (quasar_settings_t*) &ext->GetSettings();
    }

    return nullptr;
}

quasar_selection_options_t* quasar_create_selection_setting(void)
{
    return (quasar_selection_options_t*) new SelectionOptionsVector{};
}

quasar_data_handle quasar_set_data_string(quasar_data_handle hData, const char* data)
{
    quasar_return_data_t* ref = static_cast<quasar_return_data_t*>(hData);

    if (ref)
    {
        ref->val = std::string{data};

        return ref;
    }

    return nullptr;
}

template<typename T, typename = std::enable_if_t<std::is_same_v<double, T> || std::is_same_v<int, T> || std::is_same_v<bool, T>, T>>
quasar_data_handle _set_basic_json_type(quasar_data_handle hData, T data)
{
    quasar_return_data_t* ref = static_cast<quasar_return_data_t*>(hData);

    if (ref)
    {
        ref->val = data;

        return ref;
    }

    return nullptr;
}

quasar_data_handle quasar_set_data_int(quasar_data_handle hData, int data)
{
    return _set_basic_json_type(hData, data);
}

quasar_data_handle quasar_set_data_double(quasar_data_handle hData, double data)
{
    return _set_basic_json_type(hData, data);
}

quasar_data_handle quasar_set_data_bool(quasar_data_handle hData, bool data)
{
    return _set_basic_json_type(hData, data);
}

quasar_data_handle quasar_set_data_json(quasar_data_handle hData, const char* data)
{
    quasar_return_data_t* ref = static_cast<quasar_return_data_t*>(hData);

    if (ref)
    {
        ref->val = jsoncons::json::parse(data);

        return ref;
    }

    return nullptr;
}

quasar_data_handle quasar_set_data_string_array(quasar_data_handle hData, char** arr, size_t len)
{
    quasar_return_data_t* ref = static_cast<quasar_return_data_t*>(hData);

    if (ref)
    {
        ref->val = jsoncons::json{jsoncons::json_array_arg};

        std::transform(arr, arr + len, ref->val.value().begin_elements(), [](auto elm) {
            return std::string{elm};
        });

        return ref;
    }

    return nullptr;
}

template<typename T, typename = std::enable_if_t<std::is_same_v<double, T> || std::is_same_v<int, T> || std::is_same_v<float, T>, T>>
quasar_data_handle _copy_basic_array(quasar_data_handle hData, T* arr, size_t len)
{
    quasar_return_data_t* ref = static_cast<quasar_return_data_t*>(hData);

    if (ref)
    {
        ref->val = jsoncons::json{jsoncons::json_array_arg};

        std::copy(arr, arr + len, ref->val.value().begin_elements());

        return ref;
    }

    return nullptr;
}

quasar_data_handle quasar_set_data_int_array(quasar_data_handle hData, int* arr, size_t len)
{
    return _copy_basic_array(hData, arr, len);
}

quasar_data_handle quasar_set_data_float_array(quasar_data_handle hData, float* arr, size_t len)
{
    return _copy_basic_array(hData, arr, len);
}

quasar_data_handle quasar_set_data_double_array(quasar_data_handle hData, double* arr, size_t len)
{
    return _copy_basic_array(hData, arr, len);
}

quasar_data_handle quasar_set_data_null(quasar_data_handle hData)
{
    quasar_return_data_t* ref = static_cast<quasar_return_data_t*>(hData);

    if (ref)
    {
        ref->val = jsoncons::json::null();

        return ref;
    }

    return nullptr;
}

quasar_data_handle quasar_append_error(quasar_data_handle hData, const char* err)
{
    quasar_return_data_t* ref = static_cast<quasar_return_data_t*>(hData);

    if (ref)
    {
        ref->errors.push_back(err);

        return ref;
    }

    return nullptr;
}

quasar_settings_t* quasar_add_int_setting(quasar_settings_t* settings, const char* name, const char* description, int min, int max, int step, int dflt)
{
    SettingsVariantVector* container = reinterpret_cast<SettingsVariantVector*>(settings);

    if (container)
    {
        Settings::Setting<int> entry{name, description, dflt, min, max, step};

        container->push_back(entry);
        return settings;
    }

    return nullptr;
}

quasar_settings_t* quasar_add_bool_setting(quasar_settings_t* settings, const char* name, const char* description, bool dflt)
{
    SettingsVariantVector* container = reinterpret_cast<SettingsVariantVector*>(settings);

    if (container)
    {
        Settings::Setting<bool> entry{name, description, dflt};

        container->push_back(entry);
        return settings;
    }

    return nullptr;
}

quasar_settings_t*
quasar_add_double_setting(quasar_settings_t* settings, const char* name, const char* description, double min, double max, double step, double dflt)
{
    SettingsVariantVector* container = reinterpret_cast<SettingsVariantVector*>(settings);

    if (container)
    {
        Settings::Setting<double> entry{name, description, dflt, min, max, step};

        container->push_back(entry);
        return settings;
    }

    return nullptr;
}

quasar_settings_t* quasar_add_string_setting(quasar_settings_t* settings, const char* name, const char* description, const char* dflt, bool password)
{
    SettingsVariantVector* container = reinterpret_cast<SettingsVariantVector*>(settings);

    if (container)
    {
        Settings::Setting<std::string> entry{name, description, dflt, password};

        container->push_back(entry);
        return settings;
    }

    return nullptr;
}

quasar_settings_t* quasar_add_selection_setting(quasar_settings_t* settings, const char* name, const char* description, quasar_selection_options_t* select)
{
    SettingsVariantVector*  set_con = reinterpret_cast<SettingsVariantVector*>(settings);
    SelectionOptionsVector* sel_con = reinterpret_cast<SelectionOptionsVector*>(select);

    if (set_con && sel_con)
    {
        if (sel_con->empty())
        {
            SPDLOG_WARN("Failed to create option {}. Selection has no options.", name);
            delete sel_con;
            return nullptr;
        }

        Settings::SelectionSetting<std::string> entry{name, description, sel_con->at(0).first, *sel_con};
        set_con->push_back(entry);
        delete sel_con;

        select = nullptr;

        return settings;
    }

    return nullptr;
}

quasar_selection_options_t* quasar_add_selection_option(quasar_selection_options_t* select, const char* name, const char* value)
{
    SelectionOptionsVector* container = reinterpret_cast<SelectionOptionsVector*>(select);

    if (container)
    {
        container->push_back(std::make_pair<std::string, std::string>(value, name));

        return select;
    }

    return nullptr;
}

intmax_t quasar_get_int_setting(quasar_settings_t* settings, const char* name)
{
    SettingsVariantVector* container = reinterpret_cast<SettingsVariantVector*>(settings);

    if (container)
    {
        auto result = std::find_if(container->begin(), container->end(), [&](Settings::SettingsVariant& entry) {
            if (std::holds_alternative<Settings::Setting<int>>(entry))
            {
                auto& w = std::get<Settings::Setting<int>>(entry);
                return w.GetLabel() == name;
            }

            return false;
        });

        if (result != container->end())
        {
            auto& w = std::get<Settings::Setting<int>>(*result);

            return w.GetValue();
        }
    }

    return intmax_t();
}

uintmax_t quasar_get_uint_setting(quasar_settings_t* settings, const char* name)
{
    SettingsVariantVector* container = reinterpret_cast<SettingsVariantVector*>(settings);

    if (container)
    {
        auto result = std::find_if(container->begin(), container->end(), [&](Settings::SettingsVariant& entry) {
            if (std::holds_alternative<Settings::Setting<int>>(entry))
            {
                auto& w = std::get<Settings::Setting<int>>(entry);
                return w.GetLabel() == name;
            }

            return false;
        });

        if (result != container->end())
        {
            auto& w = std::get<Settings::Setting<int>>(*result);

            return w.GetValue();
        }
    }

    return uintmax_t();
}

bool quasar_get_bool_setting(quasar_settings_t* settings, const char* name)
{
    SettingsVariantVector* container = reinterpret_cast<SettingsVariantVector*>(settings);

    if (container)
    {
        auto result = std::find_if(container->begin(), container->end(), [&](Settings::SettingsVariant& entry) {
            if (std::holds_alternative<Settings::Setting<bool>>(entry))
            {
                auto& w = std::get<Settings::Setting<bool>>(entry);
                return w.GetLabel() == name;
            }

            return false;
        });

        if (result != container->end())
        {
            auto& w = std::get<Settings::Setting<bool>>(*result);

            return w.GetValue();
        }
    }

    return false;
}

double quasar_get_double_setting(quasar_settings_t* settings, const char* name)
{
    SettingsVariantVector* container = reinterpret_cast<SettingsVariantVector*>(settings);

    if (container)
    {
        auto result = std::find_if(container->begin(), container->end(), [&](Settings::SettingsVariant& entry) {
            if (std::holds_alternative<Settings::Setting<double>>(entry))
            {
                auto& w = std::get<Settings::Setting<double>>(entry);
                return w.GetLabel() == name;
            }

            return false;
        });

        if (result != container->end())
        {
            auto& w = std::get<Settings::Setting<double>>(*result);

            return w.GetValue();
        }
    }

    return 0.0;
}

bool quasar_get_string_setting(quasar_settings_t* settings, const char* name, char* buf, size_t size)
{
    SettingsVariantVector* container = reinterpret_cast<SettingsVariantVector*>(settings);

    if (container)
    {
        auto result = std::find_if(container->begin(), container->end(), [&](Settings::SettingsVariant& entry) {
            if (std::holds_alternative<Settings::Setting<std::string>>(entry))
            {
                auto& w = std::get<Settings::Setting<std::string>>(entry);
                return w.GetLabel() == name;
            }

            return false;
        });

        if (result != container->end())
        {
            auto& w  = std::get<Settings::Setting<std::string>>(*result);
            auto& ba = w.GetValue();

            if (size < ba.length())
            {
                SPDLOG_WARN("Buffer size for retrieving setting {} too small!", w.GetLabel());
                return false;
            }

            memcpy(buf, ba.data(), ba.length());

            return true;
        }
    }

    return false;
}
