#pragma once

#include <QObject>

QT_FORWARD_DECLARE_CLASS(WidgetRegistry)
QT_FORWARD_DECLARE_CLASS(DataServer)
QT_FORWARD_DECLARE_CLASS(AppLauncher)

class DataServices : public QObject
{
    Q_OBJECT

public:
    explicit DataServices(QObject* parent = nullptr);
    DataServices(const DataServices&) = delete;
    DataServices(DataServices&&)      = delete;
    DataServices& operator=(const DataServices&) = delete;
    DataServices& operator=(DataServices&&) = delete;

    DataServer*     getServer() { return server; }
    WidgetRegistry* getRegistry() { return reg; }
    AppLauncher*    getLauncher() { return launcher; }

private:
    // Data server
    DataServer* server;

    // Widget registry
    WidgetRegistry* reg;

    // App launcher
    AppLauncher* launcher;
};
