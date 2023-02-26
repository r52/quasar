#include "config.h"

#include <string>

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
    ReadSetting(Settings::internal.auth);
    ReadSetting(Settings::internal.cookies);
    ReadSetting(Settings::internal.loaded_widgets);
    ReadSetting(Settings::internal.lastpath);
    ReadSetting(Settings::internal.ignored_versions);
    ReadSetting(Settings::internal.applauncher);
    ReadSetting(Settings::internal.update_check);
    ReadSetting(Settings::internal.auto_update);
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

void Config::ReadDataSourceSetting(Settings::DataSourceSettings* settings)
{
    auto                         qname = QString::fromStdString(settings->name);

    Settings::DataSourceSettings cpy   = *settings;

    cfg->beginGroup(qname);
    settings->enabled = cfg->value("enabled", cpy.enabled).toBool();
    settings->rate    = cfg->value("rate", QVariant::fromValue(cpy.rate)).toLongLong();
    cfg->endGroup();
}

void Config::WriteDataSourceSetting(Settings::DataSourceSettings* const& settings)
{
    auto qname = QString::fromStdString(settings->name);

    cfg->beginGroup(qname);
    cfg->setValue("enabled", settings->enabled);
    cfg->setValue("rate", QVariant::fromValue(settings->rate));
    cfg->endGroup();
}

void Config::WriteInternalSettings()
{
    WriteSetting(Settings::internal.log_file);
    WriteSetting(Settings::internal.log_level);
    WriteSetting(Settings::internal.port);
    WriteSetting(Settings::internal.auth);
    WriteSetting(Settings::internal.cookies);
    WriteSetting(Settings::internal.loaded_widgets);
    WriteSetting(Settings::internal.lastpath);
    WriteSetting(Settings::internal.ignored_versions);
    WriteSetting(Settings::internal.applauncher);
    WriteSetting(Settings::internal.update_check);
    WriteSetting(Settings::internal.auto_update);
}
