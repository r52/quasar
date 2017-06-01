#pragma once

#include "ui_quasar.h"

#include <QMainWindow>
#include <QSystemTrayIcon>

QT_FORWARD_DECLARE_CLASS(WidgetRegistry)
QT_FORWARD_DECLARE_CLASS(DataServer)
QT_FORWARD_DECLARE_CLASS(AppLauncher)
QT_FORWARD_DECLARE_CLASS(LogWindow)
QT_FORWARD_DECLARE_CLASS(WebWidget)

class Quasar : public QMainWindow
{
    Q_OBJECT

public:
    Quasar(QWidget* parent = Q_NULLPTR);
    ~Quasar();

    WidgetRegistry* getWidgetRegistry() { return reg; };
    DataServer*     getDataServer() { return server; };
    AppLauncher*    getAppLauncher() { return launcher; };

private slots:
    void openWebWidget();
    void openConfigDialog();
    void trayIconActivated(QSystemTrayIcon::ActivationReason reason);

protected:
    virtual void closeEvent(QCloseEvent* event) Q_DECL_OVERRIDE;

private:
    void createTrayIcon();
    void createActions();

    QString getConfigKey(QString key);

private:
    Ui::QuasarClass ui;

    // Log Window
    LogWindow* logWindow;

    // Tray
    QSystemTrayIcon* trayIcon;
    QMenu*           trayIconMenu;

    // Actions/menus
    QMenu*   widgetListMenu;
    QAction* loadAction;
    QAction* settingsAction;
    QAction* logAction;
    QAction* aboutAction;
    QAction* aboutQtAction;
    QAction* quitAction;

    // Data server
    DataServer* server;

    // Widget registry
    WidgetRegistry* reg;

    // App launcher
    AppLauncher* launcher;
};
