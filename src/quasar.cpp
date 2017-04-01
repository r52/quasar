#include "quasar.h"
#include "configdialog.h"

#include <QDebug>
#include <QSystemTrayIcon>
#include <QVBoxLayout>
#include <QTextEdit>
#include <QMenu>
#include <QCloseEvent>
#include <QFileDialog>

Quasar::Quasar(QTextEdit *logWidget, QWidget *parent)
    : QMainWindow(parent), reg(this), server(this)
{
    ui.setupUi(this);

    // Setup system tray
    createActions();
    createTrayIcon();

    // Setup icon
    QIcon icon(":/Resources/q.png");
    setWindowIcon(icon);
    trayIcon->setIcon(icon);

    trayIcon->show();

    // Setup log widget
    QVBoxLayout *layout = new QVBoxLayout();
    layout->addWidget(logWidget);

    ui.centralWidget->setLayout(layout);

    resize(800, 400);

    // Load settings
    reg.loadLoadedWidgets();
}

Quasar::~Quasar()
{
}

void Quasar::openWebWidget()
{
    QString fname = QFileDialog::getOpenFileName(this, tr("Load Widget"),
        QDir::currentPath(),
        tr("Widget Definitions (*.json)"));

    reg.loadWebWidget(fname);
}

void Quasar::openConfigDialog()
{
    ConfigDialog dialog(this);
    dialog.exec();
}

void Quasar::createTrayIcon()
{
    trayIconMenu = new QMenu(this);
    trayIconMenu->addAction(loadAction);
    trayIconMenu->addAction(settingsAction);
    trayIconMenu->addAction(logAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(quitAction);

    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setContextMenu(trayIconMenu);
}

void Quasar::createActions()
{
    loadAction = new QAction(tr("&Load"), this);
    connect(loadAction, &QAction::triggered, this, &Quasar::openWebWidget);

    settingsAction = new QAction(tr("&Settings"), this);
    connect(settingsAction, &QAction::triggered, this, &Quasar::openConfigDialog);

    logAction = new QAction(tr("L&og"), this);
    connect(logAction, &QAction::triggered, this, &QWidget::showNormal);

    quitAction = new QAction(tr("&Quit"), this);
    connect(quitAction, &QAction::triggered, qApp, &QCoreApplication::quit);
}

QString Quasar::getConfigKey(QString key)
{
    return "global/" + key;
}

void Quasar::closeEvent(QCloseEvent *event)
{
#ifdef Q_OS_OSX
    if (!event->spontaneous() || !isVisible()) {
        return;
    }
#endif
    if (trayIcon->isVisible())
    {
        hide();
        event->ignore();
    }
}
