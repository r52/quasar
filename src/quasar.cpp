#include "quasar.h"

#include "preproc.h"
#include "version.h"

#include "applauncher.h"
#include "configdialog.h"
#include "dataserver.h"
#include "logwindow.h"
#include "webwidget.h"
#include "widgetregistry.h"

#include <QCloseEvent>
#include <QFileDialog>
#include <QMenu>
#include <QMessageBox>
#include <QTextEdit>
#include <QVBoxLayout>

Quasar::Quasar(QWidget* parent)
    : QMainWindow(parent), logWindow(new LogWindow(this)), server(new DataServer(this)), reg(new WidgetRegistry(this)), launcher(new AppLauncher(this))
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
    QVBoxLayout* layout = new QVBoxLayout();
    layout->addWidget(logWindow->release());

    ui.centralWidget->setLayout(layout);

    resize(800, 400);

    // Load settings
    reg->loadLoadedWidgets();
}

Quasar::~Quasar()
{
}

void Quasar::openWebWidget()
{
    QString fname = QFileDialog::getOpenFileName(this, tr("Load Widget"), QDir::currentPath(), tr("Widget Definitions (*.json)"));

    reg->loadWebWidget(fname);
}

void Quasar::openConfigDialog()
{
    ConfigDialog dialog(this);
    dialog.exec();
}

void Quasar::trayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason)
    {
        case QSystemTrayIcon::Context:
        {
            // Regenerate widget list menu
            widgetListMenu->clear();

            WidgetMapType& widgets = reg->getWidgets();

            for (WebWidget* widget : widgets)
            {
                widgetListMenu->addMenu(widget->getMenu());
            }
            break;
        }

        case QSystemTrayIcon::DoubleClick:
        {
            openWebWidget();
            break;
        }
    }
}

void Quasar::createTrayIcon()
{
    trayIconMenu = new QMenu(this);
    trayIconMenu->addAction(loadAction);
    trayIconMenu->addMenu(widgetListMenu);
    trayIconMenu->addAction(settingsAction);
    trayIconMenu->addAction(logAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(aboutAction);
    trayIconMenu->addAction(aboutQtAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(quitAction);

    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setContextMenu(trayIconMenu);

    connect(trayIcon, &QSystemTrayIcon::activated, this, &Quasar::trayIconActivated);
}

void Quasar::createActions()
{
    loadAction = new QAction(tr("&Load"), this);
    connect(loadAction, &QAction::triggered, this, &Quasar::openWebWidget);

    widgetListMenu = new QMenu(tr("Widgets"), this);

    settingsAction = new QAction(tr("&Settings"), this);
    connect(settingsAction, &QAction::triggered, this, &Quasar::openConfigDialog);

    logAction = new QAction(tr("L&og"), this);
    connect(logAction, &QAction::triggered, this, &QWidget::showNormal);

    aboutAction = new QAction(tr("&About"), this);

    connect(aboutAction, &QAction::triggered, [=](bool checked) {
        static QString aboutMsg =
            "Quasar " GIT_VER_STRING "<br/>"
#ifndef NDEBUG
            "DEBUG BUILD<br/>"
#endif
            "<br/>"
            "Third-party licenses included where applicable<br/>"
            "<br/>"
            "Compiled on: " __DATE__ "<br/>"
            "Compiler: " COMPILER_STRING "<br/>"
            "Qt version: " QT_VERSION_STR "<br/>"
            "<br/>"
            "Licensed under GPLv3<br/>"
            "Source code available at <a href='https://github.com/r52/quasar'>GitHub</a>";

        QMessageBox::about(this, tr("About Quasar"), aboutMsg);
    });

    aboutQtAction = new QAction(tr("About &Qt"), this);
    connect(aboutQtAction, &QAction::triggered, [=](bool checked) {
        QMessageBox::aboutQt(this, "About Qt");
    });

    quitAction = new QAction(tr("&Quit"), this);
    connect(quitAction, &QAction::triggered, qApp, &QCoreApplication::quit);
}

QString Quasar::getConfigKey(QString key)
{
    return "global/" + key;
}

void Quasar::closeEvent(QCloseEvent* event)
{
#ifdef Q_OS_OSX
    if (!event->spontaneous() || !isVisible())
    {
        return;
    }
#endif
    if (trayIcon->isVisible())
    {
        hide();
        event->ignore();
    }
}
