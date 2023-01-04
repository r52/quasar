#pragma once

#include <memory>

#include "settings.h"

#include <QSettings>
#include <QVariant>

class QSettings;

class Config
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

    template<typename T, bool ranged>
    void ReadSetting(Settings::Setting<T, ranged>& setting) const
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
    void WriteSetting(Settings::Setting<T, ranged>& setting)
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

private:
    void                       WriteInternalSettings();

    std::unique_ptr<QSettings> cfg{};
};
