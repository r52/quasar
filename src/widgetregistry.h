#pragma once

#include <qstring_hash_impl.h>

#include <QObject>
#include <memory>
#include <unordered_map>

QT_FORWARD_DECLARE_CLASS(WebWidget);

using WidgetMapType = std::unordered_map<QString, std::unique_ptr<WebWidget>>;

class WidgetRegistry : public QObject
{
    Q_OBJECT

public:
    WidgetRegistry(QObject* parent = Q_NULLPTR);
    ~WidgetRegistry();

    bool loadWebWidget(QString filename, bool userAction = true);

    WidgetMapType& getWidgets() { return m_widgetMap; }

    WebWidget* findWidget(QString widgetName);

private:
    void loadCookies();

public slots:
    void closeWebWidget(WebWidget* widget);

private:
    WidgetMapType m_widgetMap;
};
