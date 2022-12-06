#include "config.h"

#include <string>

#include <QSettings>

Config::Config() : cfg{std::make_unique<QSettings>()}
{
    ReadInteralSettings();
}

Config::~Config()
{
    Save();
}

void Config::Save()
{
    WriteInternalSettings();
}

void Config::ReadInteralSettings()
{
    ReadSetting(Settings::internal.log_file);
    ReadSetting(Settings::internal.log_level);
    ReadSetting(Settings::internal.port);
    ReadSetting(Settings::internal.loaded_widgets);
}

QByteArray Config::ReadGeometry(const QString& name)
{
    const auto geometry = cfg->value(name + "/geometry", QByteArray()).toByteArray();

    return geometry;
}

void Config::WriteGeometry(const QString& name, const QByteArray& geometry)
{
    cfg->setValue(name + "/geometry", geometry);
}

void Config::WriteInternalSettings()
{
    WriteSetting(Settings::internal.log_file);
    WriteSetting(Settings::internal.log_level);
    WriteSetting(Settings::internal.port);
    WriteSetting(Settings::internal.loaded_widgets);
}

template<typename T, bool ranged>
void Config::ReadSetting(Settings::Setting<T, ranged>& setting) const
{
    QString  name = QString::fromStdString(setting.GetLabel());
    QVariant val  = cfg->value(name, QVariant::fromValue<T>(setting.GetDefault()));

    if constexpr (std::same_as<T, std::string>)
    {
        setting.SetValue(val.toString().toStdString());
    }
    else
    {
        setting.SetValue(val.value<T>());
    }
}

template<typename T, bool ranged>
void Config::WriteSetting(Settings::Setting<T, ranged>& setting)
{
    QString  name = QString::fromStdString(setting.GetLabel());

    QVariant result;

    if constexpr (std::same_as<T, std::string>)
    {
        result = QString::fromStdString(setting.GetValue());
    }
    else
    {
        result = QVariant::fromValue<T>(setting.GetValue());
    }

    cfg->setValue(name, result);
}
