#pragma once

#include <QMap>
#include <QObject>

QT_FORWARD_DECLARE_CLASS(WebWidget);

using WidgetMapType = QMap<QString, WebWidget*>;

class WidgetRegistry : public QObject
{
    Q_OBJECT

public:
    WidgetRegistry(QObject* parent = Q_NULLPTR);
    ~WidgetRegistry();

    bool loadWebWidget(QString filename, bool warnSecurity = true);

    WidgetMapType& getWidgets() { return m_widgetMap; }

    WebWidget* findWidget(QString widgetName);

private:
    void loadCookies();

public slots:
    void closeWebWidget(WebWidget* widget);

private:
    WidgetMapType m_widgetMap;
};
