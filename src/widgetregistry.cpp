#include "widgetregistry.h"

#include "webwidget.h"
#include "widgetdefs.h"

#include <QNetworkCookie>
#include <QtWebEngineCore/QWebEngineCookieStore>
#include <QtWebEngineWidgets/QWebEngineProfile>

namespace
{
    enum NetscapeCookieFormat
    {
        NETSCAPE_COOKIE_DOMAIN = 0,
        NETSCAPE_COOKIE_FLAG,
        NETSCAPE_COOKIE_PATH,
        NETSCAPE_COOKIE_SECURE,
        NETSCAPE_COOKIE_EXP,
        NETSCAPE_COOKIE_NAME,
        NETSCAPE_COOKIE_VALUE,
        NETSCAPE_COOKIE_MAX
    };
}

WidgetRegistry::WidgetRegistry(QObject* parent)
    : QObject(parent)
{
    loadCookies();
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
    QSettings   settings;
    QStringList loadedList = settings.value(QUASAR_CONFIG_LOADED).toStringList();

    foreach (const QString& f, loadedList)
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

        QByteArray    wgtDat = wgtFile.readAll();
        QJsonDocument loadDoc(QJsonDocument::fromJson(wgtDat));

        QJsonObject dat       = loadDoc.object();
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
            QString defName    = dat[WGT_DEF_NAME].toString();
            QString widgetName = defName;
            int     idx        = 2;

            while (m_widgetMap.count(widgetName) > 0)
            {
                widgetName = defName + QString::number(idx++);
            }

            qInfo() << "Loading widget " << widgetName << " (" << dat[WGT_DEF_FULLPATH].toString() << ")";

            WebWidget* widget = new WebWidget(widgetName, dat);

            m_widgetMap.insert(widgetName, widget);

            connect(widget, &WebWidget::WebWidgetClosed, this, &WidgetRegistry::closeWebWidget);
            widget->show();

            // Add to loaded
            QSettings   settings;
            QStringList loaded = settings.value(QUASAR_CONFIG_LOADED).toStringList();
            loaded.append(widget->getFullPath());
            settings.setValue(QUASAR_CONFIG_LOADED, loaded);

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

void WidgetRegistry::loadCookies()
{
    QSettings settings;
    QString   cookiesfile = settings.value(QUASAR_CONFIG_COOKIES).toString();

    if (!cookiesfile.isEmpty())
    {
        QFile file(cookiesfile);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            qWarning() << "Failed to load cookies file " << cookiesfile;
        }
        else
        {
            QWebEngineCookieStore* store = QWebEngineProfile::defaultProfile()->cookieStore();

            // parse netscape format cookies file
            QTextStream in(&file);

            while (!in.atEnd())
            {
                QString line = in.readLine();

                if (line.at(0) == '#')
                {
                    // skip comments
                    continue;
                }

                QStringList vals = line.split('\t');

                if (vals.count() != NETSCAPE_COOKIE_MAX)
                {
                    // ill formatted line
                    qDebug() << "Ill formatted cookie \"" << line << "\"";
                    continue;
                }

                QNetworkCookie cookie(vals[NETSCAPE_COOKIE_NAME].toUtf8(), vals[NETSCAPE_COOKIE_VALUE].toUtf8());
                cookie.setDomain(vals[NETSCAPE_COOKIE_DOMAIN]);
                cookie.setExpirationDate(QDateTime::fromSecsSinceEpoch(vals[NETSCAPE_COOKIE_EXP].toLongLong()));
                cookie.setPath(vals[NETSCAPE_COOKIE_PATH]);
                cookie.setSecure(vals[NETSCAPE_COOKIE_SECURE] == "TRUE");

                store->setCookie(cookie);
            }

            qInfo() << "Cookies file " << cookiesfile << "loaded";
        }
    }
    else
    {
        qInfo() << "No cookies loaded";
    }
}

void WidgetRegistry::closeWebWidget(WebWidget* widget)
{
    // Remove from loaded list
    QJsonObject data = widget->getData();
    QString     name = widget->getName();

    qInfo() << "Closing widget " << name << " (" << data[WGT_DEF_FULLPATH].toString() << ")";

    widget->saveSettings();

    // Remove from loaded
    QSettings   settings;
    QStringList loaded = settings.value(QUASAR_CONFIG_LOADED).toStringList();
    loaded.removeAll(widget->getFullPath());
    settings.setValue(QUASAR_CONFIG_LOADED, loaded);

    // Remove from registry
    auto it = m_widgetMap.find(name);

    if (it != m_widgetMap.end())
    {
        Q_ASSERT((*it) == widget);
        m_widgetMap.erase(it);
    }

    widget->deleteLater();
}
