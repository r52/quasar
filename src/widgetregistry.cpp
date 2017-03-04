#include "widgetregistry.h"
#include "widgetdefs.h"
#include "webwidget.h"

WidgetRegistry::WidgetRegistry()
{
}

WidgetRegistry::~WidgetRegistry()
{
    // Save loaded list
    QSettings settings;
    settings.setValue("global/loaded", QStringList(map.keys()));

    // Delete alive widgets and clear list
    QMapIterator<QString, WebWidget*> it(map);

    while (it.hasNext())
    {
        auto kv = it.next();
        kv.value()->saveSettings();
        delete kv.value();
    }

    map.clear();
}

void WidgetRegistry::loadLoadedWidgets()
{
    QSettings settings;
    QStringList loadedList = settings.value("global/loaded").toStringList();

    foreach(const QString &f, loadedList)
    {
        loadWebWidget(f, false);
    }
}

bool WidgetRegistry::loadWebWidget(QString filename, bool warnSecurity)
{
    if (!filename.isNull())
    {
        if (map.count(filename) > 0)
        {
            qWarning() << "A widget with definition " << filename << " already loaded.";
            return false;
        }

        QFile wgtFile(filename);

        if (!wgtFile.open(QIODevice::ReadOnly))
        {
            qWarning() << "Failed to load '" << filename << "'";
            return false;
        }

        QByteArray wgtDat = wgtFile.readAll();
        QJsonDocument loadDoc(QJsonDocument::fromJson(wgtDat));

        QJsonObject dat = loadDoc.object();
        dat[WGT_DEF_FULLPATH] = filename;

        if (!WebWidget::validateWidgetDefinition(dat))
        {
            qWarning() << "Invalid widget definition '" << filename << "'";
        }
        else if (warnSecurity && !WebWidget::acceptSecurityWarnings(dat))
        {
            qWarning() << "Denied loading '" << filename << "'";
        }
        else
        {
            qInfo() << "Loading widget " << dat[WGT_DEF_NAME].toString() << " (" << dat[WGT_DEF_FULLPATH].toString() << ")";
            WebWidget *widget = new WebWidget(dat);

            map.insert(dat[WGT_DEF_FULLPATH].toString(), widget);

            connect(widget, &WebWidget::WebWidgetClosed, this, &WidgetRegistry::closeWebWidget);
            widget->show();

            return true;
        }
    }

    return false;
}

void WidgetRegistry::closeWebWidget(WebWidget* widget)
{
    // Remove from loaded list
    QJsonObject data = widget->getData();

    qInfo() << "Closing widget " << data[WGT_DEF_NAME].toString() << " (" << data[WGT_DEF_FULLPATH].toString() << ")";

    // Remove from registry
    auto it = map.find(data[WGT_DEF_FULLPATH].toString());

    if (it != map.end())
    {
        Q_ASSERT((*it) == widget);
        map.erase(it);
    }

    widget->deleteLater();
}
