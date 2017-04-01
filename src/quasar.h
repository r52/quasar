#pragma once

#include "ui_quasar.h"
#include "widgetregistry.h"
#include "dataserver.h"

#include <QtWidgets/QMainWindow>
#include <QSystemTrayIcon>

QT_FORWARD_DECLARE_CLASS(QTextEdit);
QT_FORWARD_DECLARE_CLASS(WebWidget);

class Quasar : public QMainWindow
{
    Q_OBJECT

public:
    Quasar(QTextEdit *logWidget, QWidget *parent = Q_NULLPTR);
    ~Quasar();

    WidgetRegistry& getWidgetRegistry() { return reg; };
    DataServer& getDataServer() { return server; };

private slots:
    void openWebWidget();
    void openConfigDialog();
    void trayIconActivated(QSystemTrayIcon::ActivationReason reason);

protected:
    virtual void closeEvent(QCloseEvent *event) Q_DECL_OVERRIDE;

private:
    void createTrayIcon();
    void createActions();

    QString getConfigKey(QString key);

private:
    Ui::QuasarClass ui;

    // Tray
    QSystemTrayIcon *trayIcon;
    QMenu *trayIconMenu;

    // Actions/menus
    QMenu *widgetListMenu;
    QAction *loadAction;
    QAction *settingsAction;
    QAction *logAction;
    QAction *quitAction;

    // Widget registry
    WidgetRegistry reg;

    // Data server
    DataServer server;
};
