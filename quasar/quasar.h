#pragma once

#include "ui_quasar.h"

#include <QSystemTrayIcon>
#include <QTextEdit>
#include <QtWidgets/QMainWindow>

class Config;
class Server;
class WidgetManager;

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

    void initializeLogger(QTextEdit* edit);

private slots:
    void openWidget();
    void trayIconActivated(QSystemTrayIcon::ActivationReason reason);

private:
    Ui::QuasarClass ui;

    // Tray
    QSystemTrayIcon* trayIcon{};
    QMenu*           trayIconMenu{};
    QMenu*           widgetListMenu{};
    QAction*         loadAction{};
    QAction*         settingsAction{};
    QAction*         logAction{};
    QAction*         consoleAction{};
    QAction*         aboutAction{};
    QAction*         aboutQtAction{};
    QAction*         docAction{};
    QAction*         quitAction{};

    //
    std::shared_ptr<Config>        config{};
    std::shared_ptr<Server>        server{};
    std::shared_ptr<WidgetManager> manager{};
};
