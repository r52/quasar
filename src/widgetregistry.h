#pragma once

#include "sharedlocker.h"
#include <qstring_hash_impl.h>

#include <QObject>
#include <memory>
#include <unordered_map>

QT_FORWARD_DECLARE_CLASS(WebWidget);
QT_FORWARD_DECLARE_CLASS(DataServer)

using WidgetMapType = std::unordered_map<QString, std::unique_ptr<WebWidget>>;

class WidgetRegistry : public QObject
{
    friend class DataServices;

    Q_OBJECT

public:
    ~WidgetRegistry();

    bool loadWebWidget(QString filename, bool userAction, QWidget* parent = nullptr);

    auto getWidgets() { return make_shared_locker<WidgetMapType>(&m_widgetMap, &m_mutex); }

private:
    void loadCookies();

public slots:
    void closeWebWidget(WebWidget* widget);

private:
    explicit WidgetRegistry(DataServer* s, QObject* parent = nullptr);
    WidgetRegistry()                      = delete;
    WidgetRegistry(const WidgetRegistry&) = delete;
    WidgetRegistry(WidgetRegistry&&)      = delete;
    WidgetRegistry& operator=(const WidgetRegistry&) = delete;
    WidgetRegistry& operator=(WidgetRegistry&&) = delete;

    WidgetMapType             m_widgetMap;
    mutable std::shared_mutex m_mutex;
    DataServer*               server;
};
