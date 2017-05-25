#pragma once

#include <QVariant>

#include <shared_mutex>

QT_FORWARD_DECLARE_CLASS(QWebSocket)
QT_FORWARD_DECLARE_CLASS(Quasar)

class AppLauncher : public QObject
{
    Q_OBJECT;

public:
    explicit AppLauncher(QObject* parent);
    ~AppLauncher();

    const QVariantMap* getMapForRead();
    void               releaseMap(const QVariantMap*& map);

    void writeMap(QVariantMap& newmap);

private:
    void handleCommand(const QJsonObject& req, QWebSocket* sender);

private:
    Quasar*     m_parent;
    QVariantMap m_map;

    std::shared_mutex m_mutex;
};