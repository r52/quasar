#pragma once

#include <QObject>
#include <QMap>

QT_FORWARD_DECLARE_CLASS(WebWidget);

class WidgetRegistry : public QObject
{
    Q_OBJECT

public:
    WidgetRegistry();
    ~WidgetRegistry();

    void loadLoadedWidgets();
    bool loadWebWidget(QString filename, bool warnSecurity = true);

    WebWidget* findWidgetByName(QString widgetName);

public slots:
    void closeWebWidget(WebWidget* widget);

private:
    QMap<QString, WebWidget*> map;
    QMap<QString, QString> name_to_def_map;
};
