#pragma once

#include <algorithm>
#include <concepts>
#include <map>
#include <string>
#include <tuple>
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

    template<typename T>
    concept SettingTypes = std::integral<T> || std::floating_point<T> || std::is_convertible_v<T, std::string_view>;

    // Settings class adapted from https://github.com/yuzu-emu/yuzu
    template<SettingTypes T, bool ranged = IsRanged<T>>
    class Setting
    {
    protected:
        Setting() = default;

        explicit Setting(const T& val) : value{val} {}

    public:
        explicit Setting(const std::string& name, const std::string& desc, const T& default_val) requires(!ranged)
            : value{default_val}, default_value{default_val}, label{name}, description{desc}
        {}

        explicit Setting(const std::string& name, const std::string& desc, const T& default_val, const bool is_password)
            requires(!ranged && std::is_convertible_v<T, std::string_view>)
            : value{default_val}, default_value{default_val}, label{name}, description{desc}, password{is_password}
        {}

        virtual ~Setting() = default;

        explicit Setting(const std::string& name, const std::string& desc, const T& default_val, const T& min_val, const T& max_val, const T& stp)
            requires(ranged)
            : value{default_val}, default_value{default_val}, maximum{max_val}, minimum{min_val}, label{name}, description{desc}, step{stp}
        {}

        virtual const T& GetValue() const { return value; }

        virtual void     SetValue(const T& val)
        {
            T temp{ranged ? std::clamp(val, minimum, maximum) : val};
            std::swap(value, temp);
        }

        const T&            GetDefault() const { return default_value; }

        std::tuple<T, T, T> GetMinMaxStep() const { return std::make_tuple(minimum, maximum, step); }

        const bool          GetIsPassword() const { return password; }

        const std::string&  GetLabel() const { return label; }

        const std::string&  GetDescription() const { return description; }

        virtual const T&    operator= (const T& val)
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
        const T           step{};
        const bool        password{};
        const std::string label{};
        const std::string description{};
    };

    template<SettingTypes T, bool ranged = false>
    class SelectionSetting : virtual public Setting<T, ranged>
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
        Setting<int>         port{"main/port", "WebSocket server port", 13337, 1000, 65535, 1};
        Setting<std::string> loaded_widgets{"main/loaded", "Loaded Widgets", ""};
        Setting<std::string> lastpath{"main/lastpath", "Last used file path", ""};

        Setting<std::string> applauncher{"applauncher/list", "App Launcher entries", "[]"};
    };

    extern InternalSettings internal;

    // Widget settings
    struct WidgetSettings
    {
        bool  alwaysOnTop;
        bool  fixedPosition;
        bool  clickable;
        QSize customSize;
        QSize defaultSize;
    };

    struct AppLauncherData
    {
        std::string command;
        std::string file;
        std::string start;
        std::string args;
        std::string icon;
    };

    struct DataSourceSettings
    {
        std::string name;
        bool        enabled;
        int64_t     rate;
    };

    struct ExtensionInfo
    {
        std::string name;
        std::string fullname;
        std::string description;
        std::string author;
        std::string version;
        std::string url;
    };

    using SettingsVariant = std::variant<Setting<int>, Setting<double>, Setting<bool>, Setting<std::string>, SelectionSetting<std::string>>;

    extern std::unordered_map<std::string, std::tuple<ExtensionInfo, std::vector<DataSourceSettings*>, std::vector<SettingsVariant>*>> extension;

}  // namespace Settings

// Template instatiators for supported types
// This needs to be here to provide plugin support at runtime for types
// not currently used internally
template class Settings::Setting<int>;
template class Settings::Setting<double>;
template class Settings::Setting<bool>;
template class Settings::Setting<std::string>;
template class Settings::SelectionSetting<int>;
template class Settings::SelectionSetting<double>;
template class Settings::SelectionSetting<std::string>;
