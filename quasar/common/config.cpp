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
    ReadSetting(Settings::internal.lastpath);
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

Settings::WidgetSettings Config::ReadWidgetSettings(const QString& name, Settings::WidgetSettings& settings) const
{
    Settings::WidgetSettings ws;

    cfg->beginGroup(name);
    ws.alwaysOnTop   = cfg->value("alwaysOnTop").toBool();
    ws.fixedPosition = cfg->value("fixedPosition").toBool();
    ws.clickable     = cfg->value("clickable", settings.clickable).toBool();
    ws.customSize    = cfg->value("customSize", settings.defaultSize).toSize();
    cfg->endGroup();

    return ws;
}

void Config::WriteWidgetSettings(const QString& name, const Settings::WidgetSettings& settings)
{
    cfg->beginGroup(name);
    cfg->setValue("alwaysOnTop", settings.alwaysOnTop);
    cfg->setValue("fixedPosition", settings.fixedPosition);
    cfg->setValue("clickable", settings.clickable);
    cfg->setValue("customSize", settings.customSize);
    cfg->endGroup();
}

void Config::AddDataSourceSetting(const std::string& name, Settings::DataSourceSettings* settings)
{
    auto qname = QString::fromStdString(name);

    if (!Settings::datasource.contains(name))
    {
        Settings::datasource.insert({name, settings});
    }

    Settings::DataSourceSettings cpy = *settings;

    cfg->beginGroup(qname);
    settings->enabled = cfg->value("enabled", cpy.enabled).toBool();
    settings->rate    = cfg->value("rate", QVariant{cpy.rate}).toLongLong();
    cfg->endGroup();
}

void Config::WriteDataSourceSetting(const std::string& name, Settings::DataSourceSettings* const& settings)
{
    auto qname = QString::fromStdString(name);

    cfg->beginGroup(qname);
    cfg->setValue("enabled", settings->enabled);
    cfg->setValue("rate", QVariant{settings->rate});
    cfg->endGroup();
}

void Config::WriteInternalSettings()
{
    WriteSetting(Settings::internal.log_file);
    WriteSetting(Settings::internal.log_level);
    WriteSetting(Settings::internal.port);
    WriteSetting(Settings::internal.loaded_widgets);
    WriteSetting(Settings::internal.lastpath);
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
