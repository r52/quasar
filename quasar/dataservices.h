#pragma once

#include <QObject>

QT_FORWARD_DECLARE_CLASS(WidgetRegistry)
QT_FORWARD_DECLARE_CLASS(DataServer)

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

private:
    // Data server
    DataServer* server;

    // Widget registry
    WidgetRegistry* reg;
};
