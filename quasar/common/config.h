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

    template<typename T>
    [[nodiscard]] T ReadGenericStorage(const std::string& group, const std::string& label) const
    {
        QString gname = QString::fromStdString(group);
        QString lname = QString::fromStdString(label);

        cfg->beginGroup(gname);
        QVariant val = cfg->value(lname);
        cfg->endGroup();

        if constexpr (std::same_as<T, std::string>)
        {
            return val.toString().toStdString();
        }
        else
        {
            return val.value<T>();
        }
    }

    template<typename T>
    void WriteGenericStorage(const std::string& group, const std::string& label, const T& val)
    {
        QString gname = QString::fromStdString(group);
        QString lname = QString::fromStdString(label);

        cfg->beginGroup(gname);

        QVariant result;

        if constexpr (std::same_as<T, std::string>)
        {
            result = QString::fromStdString(val);
        }
        else
        {
            result = QVariant::fromValue<T>(val);
        }

        cfg->setValue(label, result);

        cfg->endGroup();
    }

private:
    void                       WriteInternalSettings();

    std::unique_ptr<QSettings> cfg{};
};
