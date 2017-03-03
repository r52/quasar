#pragma once

#include "ui_quasar.h"
#include "widgetregistry.h"

#include <QtWidgets/QMainWindow>

QT_FORWARD_DECLARE_CLASS(QSystemTrayIcon);
QT_FORWARD_DECLARE_CLASS(QTextEdit);
class WebWidget;

class Quasar : public QMainWindow
{
    Q_OBJECT

public:
    Quasar(Quasar *&inst, QWidget *parent = Q_NULLPTR);
    ~Quasar();

    void logMessage(QtMsgType type, const QMessageLogContext &context, const QString &msg);

private slots:
    void openWebWidget();

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

    // Actions
    QAction *loadAction;
    QAction *logAction;
    QAction *quitAction;
    
    // Log widget
    QTextEdit *logEdit;

    // Widget registry
    WidgetRegistry reg;
};
