#include "api/extension_support.h"
#include "extension_support_internal.h"

#include <spdlog/spdlog.h>

#include <daw/json/daw_json_link.h>

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
        case QUASAR_LOG_CRITICAL:
            SPDLOG_CRITICAL(msg);
            break;
    }
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
