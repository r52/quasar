#pragma once

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

    bool loadWebWidget(QString filename, bool userAction = true);

    WidgetMapType& getWidgets() { return m_widgetMap; }

    WebWidget* findWidget(QString widgetName);

private:
    void loadCookies();

public slots:
    void closeWebWidget(WebWidget* widget);

private:
    explicit WidgetRegistry(DataServer* s, QObject* parent = Q_NULLPTR);
    WidgetRegistry(const WidgetRegistry&) = delete;
    WidgetRegistry(WidgetRegistry&&)      = delete;
    WidgetRegistry& operator=(const WidgetRegistry&) = delete;
    WidgetRegistry& operator=(WidgetRegistry&&) = delete;

    WidgetMapType m_widgetMap;

    DataServer* server;
};
