#pragma once

#include "ui_quasar.h"

#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QSystemTrayIcon>

QT_FORWARD_DECLARE_CLASS(DataServices)
QT_FORWARD_DECLARE_CLASS(LogWindow)
QT_FORWARD_DECLARE_CLASS(WebWidget)
QT_FORWARD_DECLARE_CLASS(WebUiDialog)

class Quasar : public QMainWindow
{
    Q_OBJECT

public:
    explicit Quasar(LogWindow* log, DataServices* s, QWidget* parent = nullptr);
    ~Quasar();

protected:
    virtual void closeEvent(QCloseEvent* event) Q_DECL_OVERRIDE;

private:
    void createTrayIcon();
    void createActions();
    void checkForUpdates();

private slots:
    void openWebWidget();
    void trayIconActivated(QSystemTrayIcon::ActivationReason reason);

private:
    Quasar(const Quasar&) = delete;
    Quasar(Quasar&&)      = delete;
    Quasar& operator=(const Quasar&) = delete;
    Quasar& operator=(Quasar&&) = delete;

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
    QAction* consoleAction;
    QAction* aboutAction;
    QAction* aboutQtAction;
    QAction* docAction;
    QAction* quitAction;

    // Services
    DataServices* service;

    // WebUI dialogs
    WebUiDialog* setdlg;
    WebUiDialog* condlg;

    // Networking
    QNetworkAccessManager* netmanager;
    QNetworkRequest        updrequest;
};
