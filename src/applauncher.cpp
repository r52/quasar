#include "applauncher.h"

#include "dataserver.h"
#include "quasar.h"
#include "widgetregistry.h"

#include <QDesktopServices>
#include <QFileInfo>
#include <QJsonObject>
#include <QProcess>
#include <QSettings>
#include <QtWebSockets/QWebSocket>

AppLauncher::AppLauncher(QObject* parent)
    : QObject(parent), m_parent(qobject_cast<Quasar*>(parent))
{
    qRegisterMetaType<AppLauncherData>("AppLauncherData");
    qRegisterMetaTypeStreamOperators<AppLauncherData>("AppLauncherData");

    if (nullptr == m_parent)
    {
        throw std::invalid_argument("Parent must be a Quasar window");
    }

    using namespace std::placeholders;
    m_parent->getDataServer()->addHandler("launcher", std::bind(&AppLauncher::handleCommand, this, _1, _2));

    QSettings settings;
    m_map = settings.value("launcher/map").toMap();
}

AppLauncher::~AppLauncher()
{
}

const QVariantMap* AppLauncher::getMapForRead()
{
    m_mutex.lock_shared();
    return &m_map;
}

void AppLauncher::releaseMap(const QVariantMap*& map)
{
    map = nullptr;
    m_mutex.unlock_shared();
}

void AppLauncher::writeMap(QVariantMap& newmap)
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_map = newmap;

    QSettings settings;
    settings.setValue("launcher/map", m_map);
}

void AppLauncher::handleCommand(const QJsonObject& req, QWebSocket* sender)
{
    QString widgetName = req["widget"].toString();
    QString app        = req["app"].toString();

    WebWidget* subWidget = m_parent->getWidgetRegistry()->findWidget(widgetName);

    if (!subWidget)
    {
        qWarning() << "Unidentified widget " << widgetName;
        return;
    }

    std::unique_lock<std::shared_mutex> lock(m_mutex);

    if (!m_map.contains(app))
    {
        qWarning() << "Launcher command " << app << " not defined";
        return;
    }

    AppLauncherData d;

    QVariant& v = m_map[app];

    if (v.canConvert<AppLauncherData>())
    {
        d = v.value<AppLauncherData>();

        QString cmd = d.file;

        if (cmd.contains("://"))
        {
            // treat as url
            qInfo() << "Launching URL " << cmd;
            QDesktopServices::openUrl(QUrl(cmd));
        }
        else
        {
            qInfo() << "Launching " << cmd << d.arguments;

            // treat as file/command
            QFileInfo info(cmd);

            if (info.exists())
            {
                cmd = info.canonicalFilePath();
            }

            bool result = QProcess::startDetached(cmd, QStringList() << d.arguments, d.startpath);

            if (!result)
            {
                qWarning() << "Failed to launch " << cmd;
            }
        }
    }
}
