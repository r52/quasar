#pragma once

#include <memory>

#include "exports.h"
#include "settings.h"

#include <QVariant>

class QSettings;

class common_API Config
{
public:
    Config();
    ~Config();

    void                     Save();

    void                     ReadInteralSettings();

    QByteArray               ReadGeometry(const QString& name);
    void                     WriteGeometry(const QString& name, const QByteArray& geometry);

    Settings::WidgetSettings ReadWidgetSettings(const QString& name, Settings::WidgetSettings& settings) const;
    void                     WriteWidgetSettings(const QString& name, const Settings::WidgetSettings& settings);

    void                     AddDataSourceSetting(const std::string& name, Settings::DataSourceSettings* settings);
    void                     WriteDataSourceSetting(const std::string& name, Settings::DataSourceSettings* const& settings);

private:
    void WriteInternalSettings();

    template<typename T, bool ranged>
    void ReadSetting(Settings::Setting<T, ranged>& setting) const;

    template<typename T, bool ranged>
    void                       WriteSetting(Settings::Setting<T, ranged>& setting);

    std::unique_ptr<QSettings> cfg{};
};
