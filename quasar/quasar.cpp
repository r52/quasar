#include "quasar.h"
#include "version.h"

#include "common/config.h"
#include "common/log.h"
#include "common/qutil.h"
#include "common/update.h"
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
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QStandardPaths>
#include <QSysInfo>
#include <QUrl>
#include <QWebEngineView>

#include <spdlog/async.h>
#include <spdlog/sinks/qt_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/spdlog.h>

#include <jsoncons/json.hpp>

namespace
{
    ConfigDialog* cfgdlg = nullptr;

    void          duplicateMenu(QMenu* dst, const QMenu& origin)
    {
        QMenu*          sub     = dst->addMenu(origin.title());
        QList<QAction*> actions = origin.actions();

        for (auto ac : actions)
        {
            QMenu* acmen = ac->menu();

            if (acmen)
            {
                duplicateMenu(sub, *acmen);
            }
            else
            {
                sub->addAction(ac);
            }
        }
    }
}  // namespace

Quasar::Quasar(QWidget* parent) : QMainWindow(parent), updateManager(new QNetworkAccessManager(this)), config{std::make_shared<Config>()}
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
    {
        throw std::runtime_error("System Tray is not supported on the current desktop manager");
    }

    ui.setupUi(this);

    connect(updateManager, &QNetworkAccessManager::finished, this, &Quasar::handleUpdateRequest);

    // Setup logger
    ui.logEdit->document()->setMaximumBlockCount(200);
    initializeLogger(ui.logEdit);

    // Initialize late components
    server  = std::make_shared<Server>(config);
    manager = std::make_shared<WidgetManager>(server);

    // Setup system tray
    createTrayMenu();
    createTrayIcon();

    manager->SetWidgetChangedCallback([this](const std::vector<QuasarWidget*>& widgets) {
        if (widgetListMenu)
        {
            // Regenerate widget list menu
            widgetListMenu->clear();

            for (auto w : widgets)
            {
                duplicateMenu(widgetListMenu, *(w->GetContextMenu()));
            }
        }
    });

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

    if (Settings::internal.update_check.GetValue())
    {
        checkForUpdates();
    }
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
    nameAction = new QAction("Quasar", this);
    QFont f    = nameAction->font();
    f.setBold(true);
    nameAction->setFont(f);

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
        auto path = QUtil::GetCommonAppDataPath();
        QDesktopServices::openUrl(QUrl(path));
    });

    logAction = new QAction(tr("L&og"), this);
    connect(logAction, &QAction::triggered, this, &QWidget::showNormal);

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

    gpuTestAction = new QAction(tr("GPU Info"), this);
    connect(gpuTestAction, &QAction::triggered, [] {
        QWebEngineView* view = new QWebEngineView();
        view->setAttribute(Qt::WA_DeleteOnClose);
        view->load(QUrl("chrome://gpu/"));
        view->show();
    });

    quitAction = new QAction(tr("&Quit"), this);
    connect(quitAction, &QAction::triggered, qApp, &QCoreApplication::exit, Qt::QueuedConnection);
}

void Quasar::createDirectories()
{
    auto path = QUtil::GetCommonAppDataPath();
    path.append("extensions/");

    QDir dir(path);
    if (!dir.exists())
    {
        // create folder
        dir.mkpath(".");
    }

#ifdef Q_OS_WIN
    // Clean up post-update
    if (Update::GetUpdateStatus() == Update::FinishedUpdating)
    {
        SPDLOG_INFO("Quasar updated to version {}", VERSION_STRING);
    }

    Update::CleanUpdateFiles();
    Update::RemoveUpdateQueue();
#endif
}

void Quasar::createTrayIcon()
{
    trayIconMenu = new QMenu(this);
    trayIconMenu->addAction(nameAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(loadAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addMenu(widgetListMenu);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(settingsAction);
    trayIconMenu->addAction(dataFolderAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(logAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(aboutAction);
    trayIconMenu->addAction(aboutQtAction);
    trayIconMenu->addAction(docAction);
    trayIconMenu->addAction(gpuTestAction);
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

void Quasar::handleUpdateRequest(QNetworkReply* reply)
{
    reply->deleteLater();

    if (reply->error())
    {
        auto errstr = reply->errorString().toStdString();
        SPDLOG_WARN("Update check failed: {} - {}", reply->error(), errstr);
        return;
    }

    if (!reply->url().toString().endsWith(".zip"))
    {
        // API query
        QString        answer = reply->readAll();

        jsoncons::json doc    = jsoncons::json::parse(answer.toStdString());

        if (doc.empty() or doc.is_null())
        {
            SPDLOG_WARN("Error parsing Github API response");
            return;
        }

        auto latver = doc.at("tag_name").as_string();
        latver.erase(0, 1);

        // simple case compare should suffice
        if (latver > VERSION_STRING)
        {
            if (QSysInfo::productType() != "windows" || !Settings::internal.auto_update.GetValue())
            {
                auto reply = QMessageBox::question(nullptr,
                    tr("Quasar Update"),
                    tr("Quasar version ") + QString::fromStdString(latver) + tr(" is available.\n\nWould you like to download it?"),
                    QMessageBox::Yes | QMessageBox::No);

                if (reply == QMessageBox::Ok)
                {
                    QDesktopServices::openUrl(QUrl(QString::fromStdString(doc.at("html_url").as_string())));
                }
                else
                {
                    // TODO Ignore version
                }
            }
            else if (QSysInfo::productType() == "windows" && Settings::internal.auto_update.GetValue())
            {
                auto assets = doc.at("assets").array_range();
                for (const auto& item : assets)
                {
                    auto item_name = item.at("name").as_string();
                    if (item_name.starts_with("quasar-windows") && item_name.ends_with(".zip"))
                    {
                        // Download the file, then queue the update
                        auto   download_url = item.at("browser_download_url").as_string();

                        QFile* dlfile       = new QFile(std::filesystem::path(item_name));
                        if (!dlfile->open(QIODevice::ReadWrite))
                        {
                            SPDLOG_WARN("Could not open file {} for writing", item_name);
                            return;
                        }

                        auto dlreply = updateManager->get(QNetworkRequest(QUrl(QString::fromStdString(download_url))));

                        connect(dlreply, &QNetworkReply::readyRead, [=] {
                            auto data = dlreply->readAll();
                            dlfile->write(data);
                        });

                        connect(dlreply, &QNetworkReply::finished, [=, this] {
                            dlreply->deleteLater();

                            if (dlreply->error())
                            {
                                dlfile->close();
                                dlfile->deleteLater();

                                auto errstr = dlreply->errorString().toStdString();
                                SPDLOG_WARN("File download failed: {} - {}", reply->error(), errstr);
                                return;
                            }

                            auto filename = dlfile->fileName();

                            auto data     = dlreply->readAll();
                            dlfile->write(data);
                            dlfile->close();
                            dlfile->deleteLater();

                            // Queue update
                            if (!Update::QueueUpdate(filename))
                            {
                                SPDLOG_WARN("Failed to queue update");
                                return;
                            }

                            nameAction->setText("Quasar " + tr("(update pending)"));
                        });

                        return;
                    }
                }
            }
        }
        else
        {
            SPDLOG_INFO("No updates available. Already on the latest version.");
        }
    }
}

void Quasar::checkForUpdates()
{
    QTimer::singleShot(5000, [this] {
        updateManager->get(QNetworkRequest(QUrl("https://api.github.com/repos/r52/quasar/releases/latest")));
    });
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
    // Clean up config dialog
    if (cfgdlg)
    {
        cfgdlg->done(QDialog::Rejected);
    }

    manager.reset();
    server.reset();
    config.reset();
}
