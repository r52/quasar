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

    void       Save();

    void       ReadInteralSettings();

    QByteArray ReadGeometry(const QString& name);
    void       WriteGeometry(const QString& name, const QByteArray& geometry);

private:
    void WriteInternalSettings();

    template<typename T, bool ranged>
    void ReadSetting(Settings::Setting<T, ranged>& setting) const;

    template<typename T, bool ranged>
    void                       WriteSetting(Settings::Setting<T, ranged>& setting);

    std::unique_ptr<QSettings> cfg{};
};
