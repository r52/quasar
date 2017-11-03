#pragma once

#include <QVariant>

#include <shared_mutex>

QT_FORWARD_DECLARE_CLASS(QWebSocket)
QT_FORWARD_DECLARE_CLASS(WidgetRegistry)
QT_FORWARD_DECLARE_CLASS(DataServer)

struct AppLauncherData
{
    QString file;
    QString startpath;
    QString arguments;

    friend QDataStream& operator<<(QDataStream& s, const AppLauncherData& o)
    {
        s << o.file;
        s << o.startpath;
        s << o.arguments;
        return s;
    }

    friend QDataStream& operator>>(QDataStream& s, AppLauncherData& o)
    {
        s >> o.file;
        s >> o.startpath;
        s >> o.arguments;
        return s;
    }
};

Q_DECLARE_METATYPE(AppLauncherData);

class AppLauncher : public QObject
{
    Q_OBJECT;

public:
    explicit AppLauncher(DataServer* s, WidgetRegistry* r, QObject* parent = Q_NULLPTR);
    ~AppLauncher();

    const QVariantMap* getMapForRead();
    void               releaseMap(const QVariantMap*& map);

    void writeMap(QVariantMap& newmap);

private:
    void handleCommand(const QJsonObject& req, QWebSocket* sender);

private:
    DataServer*       server;
    WidgetRegistry*   reg;
    QVariantMap       m_map;
    std::shared_mutex m_mutex;
};
