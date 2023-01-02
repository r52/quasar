#pragma once

#include "exports.h"

#include <algorithm>
#include <concepts>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <QSize>

namespace Settings
{

    enum LogLevel : int
    {
        trace    = 0,
        debug    = 1,
        info     = 2,
        warn     = 3,
        err      = 4,
        critical = 5,
        off      = 6,
        n_levels
    };

    template<typename T>
    concept IsRanged = (std::integral<T> && not std::same_as<T, bool>) || std::floating_point<T>;

    // Settings class adapted from https://github.com/yuzu-emu/yuzu
    template<std::movable T, bool ranged = IsRanged<T>>
    class common_API Setting
    {
    protected:
        Setting() = default;

        explicit Setting(const T& val) : value{val} {}

    public:
        explicit Setting(const std::string& name, const std::string& desc, const T& default_val) requires(!ranged)
            : value{default_val}, default_value{default_val}, label{name}, description{desc}
        {}

        virtual ~Setting() = default;

        explicit Setting(const std::string& name, const std::string& desc, const T& default_val, const T& min_val, const T& max_val) requires(ranged)
            : value{default_val}, default_value{default_val}, maximum{max_val}, minimum{min_val}, label{name}, description{desc}
        {}

        virtual const T& GetValue() const { return value; }

        virtual void     SetValue(const T& val)
        {
            T temp{ranged ? std::clamp(val, minimum, maximum) : val};
            std::swap(value, temp);
        }

        const T&           GetDefault() const { return default_value; }

        const std::string& GetLabel() const { return label; }

        const std::string& GetDescription() const { return description; }

        virtual const T&   operator= (const T& val)
        {
            T temp{ranged ? std::clamp(val, minimum, maximum) : val};
            std::swap(value, temp);
            return value;
        }

        explicit virtual operator const T& () const { return value; }

    protected:
        T                 value{};
        const T           default_value{};
        const T           maximum{};
        const T           minimum{};
        const std::string label{};
        const std::string description{};
    };

    template<std::movable T, bool ranged = false>
    class common_API SelectionSetting : virtual public Setting<T, ranged>
    {
        using SelectOption = std::pair<T, std::string>;

    public:
        explicit SelectionSetting(const std::string& name, const std::string& desc, const T& default_val, const std::vector<SelectOption>& opts) :
            Setting<T, ranged>{name, desc, default_val},
            options{opts}
        {}

        virtual ~SelectionSetting() = default;

        const std::vector<SelectOption>& GetOptions() const { return options; };

        void                             SetValue(const T& val) override
        {
            auto result = std::find_if(options.begin(), options.end(), [&val](SelectOption o) {
                return o.first == val;
            });

            if (result != options.end())
            {
                this->value = val;
                // std::swap(this->value, val);
            }
        }

        const T& operator= (const T& val) override
        {
            auto result = std::find_if(options.begin(), options.end(), [&val](SelectOption o) {
                return o.first == val;
            });

            if (result != options.end())
            {
                this->value = val;
                // std::swap(this->value, val);
            }

            return this->value;
        }

    protected:
        std::vector<SelectOption> options{};
    };

    struct InternalSettings
    {
        Setting<bool>         log_file{"main/logfile", "Log to File?", true};
        SelectionSetting<int> log_level{
            "main/loglevel",
            "Log Level",
            LogLevel::warn,
            {{LogLevel::trace, "Trace"},
                    {LogLevel::debug, "Debug"},
                    {LogLevel::info, "Info"},
                    {LogLevel::warn, "Warn"},
                    {LogLevel::err, "Error"},
                    {LogLevel::critical, "Critical"},
                    {LogLevel::off, "Off"}}
        };
        Setting<int>         port{"main/port", "WebSocket server port", 13337, 1000, 65535};
        Setting<std::string> loaded_widgets{"main/loaded", "Loaded Widgets", ""};
        Setting<std::string> lastpath{"main/lastpath", "Last used file path", ""};
    };

    extern common_API InternalSettings internal;

    // Widget settings
    struct WidgetSettings
    {
        bool  alwaysOnTop;
        bool  fixedPosition;
        bool  clickable;
        QSize customSize;
        QSize defaultSize;
    };

    // TODO extension settings
    struct DataSourceSettings
    {
        bool    enabled;
        int64_t rate;
    };

    extern common_API std::unordered_map<std::string, DataSourceSettings*> datasource;

}  // namespace Settings
