#pragma once

#include <QObject>
#include <QMap>

QT_FORWARD_DECLARE_CLASS(WebWidget);

class WidgetRegistry : public QObject
{
    Q_OBJECT

public:
    WidgetRegistry(QObject *parent = Q_NULLPTR);
    ~WidgetRegistry();

    void loadLoadedWidgets();
    bool loadWebWidget(QString filename, bool warnSecurity = true);

    WebWidget* findWidget(QString widgetName);

public slots:
    void closeWebWidget(WebWidget* widget);

private:
    QMap<QString, WebWidget*> m_widgetMap;
};
