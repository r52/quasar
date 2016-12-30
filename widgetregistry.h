#pragma once

#include <QObject>
#include <QMap>

class WebWidget;

class WidgetRegistry : public QObject
{
    Q_OBJECT

public:
    WidgetRegistry();
    ~WidgetRegistry();

    void loadLoadedWidgets();
    bool loadWebWidget(QString filename);

public slots:
    void closeWebWidget(WebWidget* widget);

private:
    QMap<QString, WebWidget*> map;
};
