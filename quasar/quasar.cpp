#include "quasar.h"
#include "version.h"

#include "common/config.h"
#include "common/log.h"
#include "common/util.h"
#include "config/configdialog.h"
#include "server/server.h"
#include "widgets/quasarwidget.h"
#include "widgets/widgetmanager.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QFileDialog>
#include <QMenu>
#include <QMessageBox>
#include <QStandardPaths>
#include <QUrl>
#include <QVBoxLayout>

#include <spdlog/async.h>
#include <spdlog/sinks/qt_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/spdlog.h>

namespace
{
    ConfigDialog* cfgdlg = nullptr;
}  // namespace

Quasar::Quasar(QWidget* parent) : QMainWindow(parent), config{std::make_shared<Config>()}
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
    {
        throw std::runtime_error("System Tray is not supported on the current desktop manager");
    }

    ui.setupUi(this);

    // Setup logger
    ui.logEdit->document()->setMaximumBlockCount(200);
    initializeLogger(ui.logEdit);

    // Initialize late components
    server  = std::make_shared<Server>(config);
    manager = std::make_shared<WidgetManager>(server);

    // Setup system tray
    createTrayMenu();
    createTrayIcon();

    // Setup icon
    QIcon icon(":/Resources/q.png");
    setWindowIcon(icon);
    trayIcon->setIcon(icon);
    trayIcon->show();

    const auto geometry = config->ReadGeometry("main");

    if (geometry.isEmpty())
    {
        resize(800, 400);
    }
    else
    {
        restoreGeometry(geometry);
    }

    createDirectories();

    // Load widgets
    manager->LoadStartupWidgets(config);
}

void Quasar::initializeLogger(QTextEdit* edit)
{
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    path.append(".log");

    constexpr auto                max_size{1048576 * 5};
    constexpr auto                max_files{3};

    auto                          qt_sink = std::make_shared<spdlog::sinks::qt_sink_mt>(edit, "append");

    std::vector<spdlog::sink_ptr> sinks{qt_sink};

    if (Settings::internal.log_file.GetValue())
    {
        auto rotating_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(path.toStdString(), max_size, max_files);
        sinks.push_back(rotating_sink);
    }

    auto logger = Log::setup_logger(sinks);

    spdlog::set_default_logger(logger);

    spdlog::set_level((spdlog::level::level_enum) Settings::internal.log_level.GetValue());
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S] [thread %t] [%^%l%$] %v - %s:L%#");

    SPDLOG_DEBUG("Log level: {}", Settings::internal.log_level.GetValue());

    if (Settings::internal.log_file.GetValue())
    {
        SPDLOG_DEBUG("Logging to: {}", path.toStdString());
    }

    SPDLOG_INFO("Log initialized!");
}

void Quasar::createTrayMenu()
{
    loadAction = new QAction(tr("&Load"), this);
    connect(loadAction, &QAction::triggered, this, &Quasar::openWidget);

    widgetListMenu = new QMenu(tr("Widgets"), this);

    settingsAction = new QAction(tr("&Settings"), this);
    connect(settingsAction, &QAction::triggered, [&] {
        if (!cfgdlg)
        {
            cfgdlg = new ConfigDialog();

            connect(cfgdlg, &QDialog::finished, [=, this](int result) {
                if (result == QDialog::Accepted)
                {
                    // Propagate settings
                    server->UpdateSettings();
                }

                cfgdlg->deleteLater();
                cfgdlg = nullptr;
            });

            cfgdlg->open();
        }
        else
        {
            cfgdlg->show();
            cfgdlg->setWindowState(Qt::WindowState::WindowActive);
        }
    });

    dataFolderAction = new QAction(tr("Open &Data Folder"), this);
    connect(dataFolderAction, &QAction::triggered, [] {
        auto path = Util::GetCommonAppDataPath();
        QDesktopServices::openUrl(QUrl(path));
    });

    logAction = new QAction(tr("L&og"), this);
    connect(logAction, &QAction::triggered, this, &QWidget::showNormal);

    // consoleAction = new QAction(tr("&Console"), this);
    // connect(consoleAction, &QAction::triggered, [&] {
    //     if (condlg == nullptr)
    //     {
    //         condlg = new WebUiDialog(service->getServer(), tr("Debug Console"), WebUiHandler::consoleUrl, CAL_DEBUG);
    //         connect(condlg, &QObject::destroyed, [&] {
    //             this->condlg = nullptr;
    //         });

    //        condlg->show();
    //    }
    //});

    aboutAction = new QAction(tr("&About Quasar"), this);

    connect(aboutAction, &QAction::triggered, [&] {
        static QString aboutMsg = "Quasar " VERSION_STRING "<br/>"
                                  "<br/>"
                                  "Compiled on: " __DATE__ "<br/>"
                                  "Qt version: " QT_VERSION_STR "<br/>"
                                  "<br/>"
                                  "Licensed under GPLv3<br/>"
                                  "Source code available at <a href='https://github.com/r52/quasar'>GitHub</a>";

        QMessageBox::about(this, tr("About Quasar"), aboutMsg);
    });

    aboutQtAction = new QAction(tr("About &Qt"), this);
    connect(aboutQtAction, &QAction::triggered, [&] {
        QMessageBox::aboutQt(this, "About Qt");
    });

    docAction = new QAction(tr("Quasar &Documentation"), this);
    connect(docAction, &QAction::triggered, [] {
        QDesktopServices::openUrl(QUrl("https://quasardoc.readthedocs.io"));
    });

    quitAction = new QAction(tr("&Quit"), this);
    connect(quitAction, &QAction::triggered, qApp, &QCoreApplication::exit, Qt::QueuedConnection);
}

void Quasar::createDirectories()
{
    auto path = Util::GetCommonAppDataPath();
    path.append("extensions/");

    QDir dir(path);
    if (!dir.exists())
    {
        // create folder
        dir.mkpath(".");
    }
}

void Quasar::createTrayIcon()
{
    trayIconMenu = new QMenu(this);
    trayIconMenu->addAction(loadAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addMenu(widgetListMenu);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(settingsAction);
    trayIconMenu->addAction(dataFolderAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(logAction);
    // trayIconMenu->addAction(consoleAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(aboutAction);
    trayIconMenu->addAction(aboutQtAction);
    trayIconMenu->addAction(docAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(quitAction);

    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setContextMenu(trayIconMenu);

    connect(trayIcon, &QSystemTrayIcon::activated, this, &Quasar::trayIconActivated);
}

void Quasar::openWidget()
{
    QString defpath  = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString lastpath = QString::fromStdString(Settings::internal.lastpath.GetValue());

    if (lastpath.isEmpty())
    {
        lastpath = defpath;
    }

    QString fname = QFileDialog::getOpenFileName(this, tr("Load Widget"), lastpath, tr("Widget Definitions (*.json)"));

    if (!fname.isNull())
    {
        QFileInfo info(fname);
        Settings::internal.lastpath.SetValue(info.canonicalPath().toStdString());

        manager->LoadWidget(fname.toStdString(), config, true);
    }
}

void Quasar::trayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason)
    {
        case QSystemTrayIcon::Trigger:
        case QSystemTrayIcon::Context:
            {
                // Regenerate widget list menu
                widgetListMenu->clear();

                {
                    auto widgets = manager->GetWidgets();

                    for (auto& w : widgets)
                    {
                        widgetListMenu->addMenu(w->GetContextMenu());
                    }
                }

                if (reason == QSystemTrayIcon::Trigger)
                {
                    trayIcon->contextMenu()->popup(QCursor::pos());
                }

                break;
            }

        case QSystemTrayIcon::DoubleClick:
            {
                openWidget();
                break;
            }
        default:
            break;
    }
}

void Quasar::closeEvent(QCloseEvent* event)
{
    config->WriteGeometry("main", saveGeometry());

#ifdef Q_OS_OSX
    if (!event->spontaneous() or !isVisible())
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

Quasar::~Quasar()
{
    manager.reset();
    server.reset();
    config.reset();
}
