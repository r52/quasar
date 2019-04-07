#include "widgetregistry.h"

#include "dataserver.h"
#include "webwidget.h"
#include "widgetdefs.h"

#include <QMessageBox>
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

WidgetRegistry::WidgetRegistry(DataServer* s, QObject* parent) : QObject(parent), server(s)
{
    loadCookies();
}

WidgetRegistry::~WidgetRegistry()
{
    m_widgetMap.clear();
}

bool WidgetRegistry::loadWebWidget(QString filename, bool userAction)
{
    if (filename.isNull())
    {
        qWarning() << "Null filename";
        return false;
    }

    QFile wgtFile(filename);

    if (!wgtFile.open(QIODevice::ReadOnly))
    {
        qWarning() << "Failed to load" << filename;
        return false;
    }

    QByteArray    wgtDat = wgtFile.readAll();
    QJsonDocument loadDoc(QJsonDocument::fromJson(wgtDat));

    QJsonObject dat       = loadDoc.object();
    dat[WGT_DEF_FULLPATH] = filename;

    if (!WebWidget::validateWidgetDefinition(dat))
    {
        qWarning() << "Invalid widget definition" << filename;
        return false;
    }

    // Verify extension dependencies
    if (dat.contains(WGT_DEF_REQUIRED))
    {
        bool    pass    = true;
        QString failext = "";
        auto    arr     = dat[WGT_DEF_REQUIRED].toArray();

        for (auto p : arr)
        {
            if (!server->findExtension(p.toString()))
            {
                pass    = false;
                failext = p.toString();
                break;
            }
        }

        if (!pass)
        {
            qWarning() << "Missing extension" << failext << "for" << filename;

            QMessageBox::warning(nullptr,
                                 tr("Missing Extension"),
                                 tr("Extension \"%1\" is required for widget \"%2\". Please install this extension and try again.").arg(failext, filename),
                                 QMessageBox::Ok);

            return false;
        }
    }

    if (userAction && !WebWidget::acceptSecurityWarnings(dat))
    {
        qWarning() << "Denied loading" << filename;
        return false;
    }

    // Generate unique widget name
    QString defName    = dat[WGT_DEF_NAME].toString();
    QString widgetName = defName;
    int     idx        = 2;

    std::unique_lock<std::shared_mutex> lk(m_mutex);

    while (m_widgetMap.count(widgetName) > 0)
    {
        widgetName = defName + QString::number(idx++);
    }

    qInfo().noquote().nospace() << "Loading widget \"" << widgetName << "\" (" << dat[WGT_DEF_FULLPATH].toString() << ")";

    WebWidget* widget = new WebWidget(widgetName, dat, server);

    m_widgetMap.insert(std::make_pair(widgetName, widget));

    connect(widget, &WebWidget::WebWidgetClosed, this, &WidgetRegistry::closeWebWidget);
    widget->show();

    if (userAction)
    {
        // Add to loaded
        QSettings   settings;
        QStringList loaded = settings.value(QUASAR_CONFIG_LOADED).toStringList();
        loaded.append(widget->getFullPath());
        settings.setValue(QUASAR_CONFIG_LOADED, loaded);
    }

    return true;
}

void WidgetRegistry::loadCookies()
{
    QSettings settings;

    auto bcookies = settings.value(QUASAR_CONFIG_COOKIES, QByteArray()).toByteArray();

    if (bcookies.isEmpty())
    {
        qInfo() << "No cookies loaded";
        return;
    }

    auto rawcookies = qUncompress(bcookies);

    if (rawcookies.isEmpty())
    {
        qWarning() << "Corrupted cookies";
        return;
    }

    QWebEngineCookieStore* store = QWebEngineProfile::defaultProfile()->cookieStore();

    // parse netscape format cookies file
    QTextStream in(rawcookies);

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

    qInfo() << "Cookies file loaded";
}

void WidgetRegistry::closeWebWidget(WebWidget* widget)
{
    // Remove from loaded list
    QString name = widget->getName();

    qInfo().noquote().nospace() << "Closing widget \"" << name << "\" (" << widget->getFullPath() << ")";

    std::unique_lock<std::shared_mutex> lk(m_mutex);

    // Remove from registry
    auto it = m_widgetMap.find(name);

    if (it != m_widgetMap.end())
    {
        Q_ASSERT((it->second.get()) == widget);
        // Release unique_ptr ownership to allow Qt gc to kick in
        it->second.release();
        m_widgetMap.erase(it);
    }

    widget->deleteLater();
}
