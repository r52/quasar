#include "widgetregistry.h"

#include "widgetdefs.h"
#include "webwidget.h"

WidgetRegistry::WidgetRegistry(QObject *parent)
    : QObject(parent)
{
}

WidgetRegistry::~WidgetRegistry()
{
    QStringList loadedWidgets;

    // Delete alive widgets and clear list
    for (WebWidget* widget : m_widgetMap)
    {
        widget->saveSettings();
        loadedWidgets << widget->getFullPath();
    }

    qDeleteAll(m_widgetMap);
    m_widgetMap.clear();

    // Save loaded list
    QSettings settings;
    settings.setValue(QUASAR_CONFIG_LOADED, loadedWidgets);
}

void WidgetRegistry::loadLoadedWidgets()
{
    QSettings settings;
    QStringList loadedList = settings.value(QUASAR_CONFIG_LOADED).toStringList();

    foreach(const QString &f, loadedList)
    {
        loadWebWidget(f, false);
    }
}

bool WidgetRegistry::loadWebWidget(QString filename, bool warnSecurity)
{
    if (!filename.isNull())
    {
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
            // Generate unique widget name
            QString defName = dat[WGT_DEF_NAME].toString();
            QString widgetName = defName;
            int idx = 2;

            while (m_widgetMap.count(widgetName) > 0)
            {
                widgetName = defName + QString::number(idx++);
            }

            qInfo() << "Loading widget " << widgetName << " (" << dat[WGT_DEF_FULLPATH].toString() << ")";

            WebWidget *widget = new WebWidget(widgetName, dat);

            m_widgetMap.insert(widgetName, widget);

            connect(widget, &WebWidget::WebWidgetClosed, this, &WidgetRegistry::closeWebWidget);
            widget->show();

            return true;
        }
    }

    return false;
}

WebWidget* WidgetRegistry::findWidget(QString widgetName)
{
    auto it = m_widgetMap.find(widgetName);

    if (it != m_widgetMap.end())
    {
        return *it;
    }

    return nullptr;
}

void WidgetRegistry::closeWebWidget(WebWidget* widget)
{
    // Remove from loaded list
    QJsonObject data = widget->getData();
    QString name = widget->getName();

    qInfo() << "Closing widget " << name << " (" << data[WGT_DEF_FULLPATH].toString() << ")";

    // Remove from registry
    auto it = m_widgetMap.find(name);

    if (it != m_widgetMap.end())
    {
        Q_ASSERT((*it) == widget);
        m_widgetMap.erase(it);
    }

    widget->deleteLater();
}
