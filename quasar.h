#pragma once

#include <QtWidgets/QMainWindow>
#include <QSystemTrayIcon>
#include <QSet>

#include "ui_quasar.h"

class QTextEdit;
class WebWidget;

class Quasar : public QMainWindow
{
    Q_OBJECT

public:
    Quasar(QWidget *parent = Q_NULLPTR);
    ~Quasar();

private slots:
    void openWebWidget();
    void closeWebWidget(WebWidget* widget);

protected:
    virtual void closeEvent(QCloseEvent *event) Q_DECL_OVERRIDE;

private:
    bool loadWebWidget(QString filename);

    void createTrayIcon();
    void createActions();

    QString getConfigKey(QString key);

private:
    Ui::QuasarClass ui;

    // Tray
    QSystemTrayIcon *trayIcon;
    QMenu *trayIconMenu;

    // Actions
    QAction *loadAction;
    QAction *logAction;
    QAction *quitAction;
    
    // Log widget
    QTextEdit *logEdit;

    // Widget registry for cleanup
    QSet<WebWidget*> webwidgets;

    // Loaded widgets list
    QStringList loadedList;
};
