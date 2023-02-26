#pragma once

#include "ui_quasar.h"

#include <QSystemTrayIcon>
#include <QTextEdit>
#include <QtWidgets/QMainWindow>

class Config;
class Server;
class WidgetManager;

class QNetworkAccessManager;
class QNetworkReply;

class Quasar : public QMainWindow
{
    Q_OBJECT

public:
    Quasar(QWidget* parent = nullptr);
    ~Quasar();

protected:
    virtual void closeEvent(QCloseEvent* event) override;

private:
    void createTrayIcon();
    void createTrayMenu();
    void createDirectories();
    void checkForUpdates();

    void initializeLogger(QTextEdit* edit);

private slots:
    void openWidget();
    void trayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void handleUpdateRequest(QNetworkReply* reply);

private:
    Ui::QuasarClass ui;

    // Tray
    QSystemTrayIcon*       trayIcon{};
    QMenu*                 trayIconMenu{};
    QMenu*                 widgetListMenu{};
    QAction*               nameAction{};
    QAction*               loadAction{};
    QAction*               settingsAction{};
    QAction*               dataFolderAction{};
    QAction*               logAction{};
    QAction*               aboutAction{};
    QAction*               aboutQtAction{};
    QAction*               docAction{};
    QAction*               gpuTestAction{};
    QAction*               quitAction{};

    QNetworkAccessManager* updateManager{};

    //
    std::shared_ptr<Config>        config{};
    std::shared_ptr<Server>        server{};
    std::shared_ptr<WidgetManager> manager{};
};
