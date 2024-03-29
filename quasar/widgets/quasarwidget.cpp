#include "quasarwidget.h"

#include "common/config.h"
#include "server/server.h"
#include "widgetmanager.h"

#include <QAbstractButton>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QLabel>
#include <QMenu>
#include <QSpinBox>
#include <QWebEngineScriptCollection>
#include <QWebEngineSettings>

#include <fmt/core.h>
#include <spdlog/spdlog.h>

QString QuasarWidget::GlobalScript{};

void    QuasarWebPage::javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level, const QString& message, int lineNumber, const QString& sourceID)
{
    std::string msg = fmt::format("CONSOLE: {} ({}:{})", message.toStdString(), sourceID.toStdString(), lineNumber);

    switch (level)
    {
        case InfoMessageLevel:
            SPDLOG_INFO(msg);
            break;
        case WarningMessageLevel:
            SPDLOG_WARN(msg);
            break;
        case ErrorMessageLevel:
            SPDLOG_ERROR(msg);
            break;
    }
}

QuasarWidget::QuasarWidget(const std::string& widgetName,
    const WidgetDefinition&                   def,
    std::shared_ptr<Server>                   serv,
    std::shared_ptr<WidgetManager>            man,
    std::shared_ptr<Config>                   cfg) :
    name{widgetName},
    widget_definition{def},
    server{serv},
    manager{man},
    config{cfg}
{
    if (name.empty())
    {
        throw std::invalid_argument("Widget name cannot be null");
    }

    if (!serv)
    {
        throw std::invalid_argument("Server cannot be null");
    }

    if (!man)
    {
        throw std::invalid_argument("WidgetManager cannot be null");
    }

    if (!cfg)
    {
        throw std::invalid_argument("Config cannot be null");
    }

    const auto qname = QString::fromStdString(name);

    // No frame/border, no taskbar button
    Qt::WindowFlags flags = Qt::FramelessWindowHint | Qt::Tool;

    webview               = new QuasarWebView(this);

    QString entryPath     = QString::fromStdString(widget_definition.startFile);
    QUrl    startFile;

    if (entryPath.startsWith("http"))
    {
        // if url, take as is
        startFile = entryPath;
    }
    else
    {
        // assume local file
        QString startFilePath = QFileInfo(QString::fromStdString(widget_definition.fullpath)).canonicalPath().append("/");
        startFile             = QUrl::fromLocalFile(startFilePath.append(entryPath));
    }

    auto page = new QuasarWebPage(this);
    page->load(startFile);
    webview->setPage(page);

    webview->settings()->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, false);
    webview->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, true);

    // Remote access permission
    if (widget_definition.remoteAccess.value_or(false))
    {
        webview->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
    }

    // Optional background transparency
    if (widget_definition.transparentBg)
    {
        setAttribute(Qt::WA_TranslucentBackground);
        webview->page()->setBackgroundColor(Qt::transparent);
    }

    /*
    // Handle geolocation access permission
    connect(page, &QWebEnginePage::featurePermissionRequested, [=](const QUrl& securityOrigin, QWebEnginePage::Feature feature) {
        if (feature != QWebEnginePage::Geolocation)
            return;
        QSettings settings;
        auto      allowgeo = settings.value(QUASAR_CONFIG_ALLOWGEO).toMap();
        auto      perm     = QWebEnginePage::PermissionDeniedByUser;
        if (allowgeo.contains(data[WGT_DEF_FULLPATH].toString()))
        {
            bool allowed = allowgeo[data[WGT_DEF_FULLPATH].toString()].toBool();
            if (allowed)
            {
                perm = QWebEnginePage::PermissionGrantedByUser;
            }
        }
        else
        {
            QMessageBox msgBox(this);
            msgBox.setText(tr("Widget %1 (%2) wants to know your location").arg(this->getName()).arg(securityOrigin.host()));
            msgBox.setInformativeText(tr("Do you want to send your current location to this widget?"));
            msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
            msgBox.setDefaultButton(QMessageBox::Yes);
            if (msgBox.exec() == QMessageBox::Yes)
            {
                perm = QWebEnginePage::PermissionGrantedByUser;
            }
            else
            {
                perm = QWebEnginePage::PermissionDeniedByUser;
            }
        }
        page->setFeaturePermission(securityOrigin, feature, perm);
        allowgeo[data[WGT_DEF_FULLPATH].toString()] = (perm == QWebEnginePage::PermissionGrantedByUser);
        settings.setValue(QUASAR_CONFIG_ALLOWGEO, allowgeo);
    });
    */

    // Overlay for catching drag and drop events
    overlay = new OverlayWidget(this);

    // Create context menu
    createContextMenuActions();
    createContextMenu();

    // Restore settings
    settings.clickable   = widget_definition.clickable.value_or(false);
    settings.defaultSize = {static_cast<int>(widget_definition.width), static_cast<int>(widget_definition.height)};

    auto cfglock         = config.lock();
    restoreGeometry(cfglock->ReadGeometry(qname));
    settings = cfglock->ReadWidgetSettings(qname, settings);

    aFixedPos->setChecked(settings.fixedPosition);
    aClickable->setChecked(settings.clickable);

    overlay->setVisible(!settings.clickable);

    if (settings.alwaysOnTop)
    {
        flags |= Qt::WindowStaysOnTopHint;
        aOnTop->setChecked(true);
    }
    else
    {
        flags |= Qt::WindowStaysOnBottomHint;
    }

    // Custom context menu
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, [&](const QPoint& pos) {
        contextMenu->exec(mapToGlobal(pos));
    });

    // Set flags
    setWindowFlags(flags);

    // resize
    webview->resize(settings.customSize);
    resize(settings.customSize);

    // Inject global script
    if (widget_definition.dataserver.value_or(false))
    {
        auto    authcode  = server.lock()->GenerateAuthCode();

        QString scriptSrc = GetGlobalScript(authcode);

        script.setName("GlobalScript");
        script.setInjectionPoint(QWebEngineScript::DocumentCreation);
        script.setWorldId(0);
        script.setSourceCode(scriptSrc);

        webview->page()->scripts().insert(script);
    }

    setWindowTitle(qname);
}

QuasarWidget::~QuasarWidget()
{
    SaveSettings();
}

QString QuasarWidget::GetGlobalScript(const std::string& authcode)
{
    if (GlobalScript.isEmpty())
    {
        QFile file(":/scripts/quasar.js");
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            throw std::runtime_error("Quasar global script load failure");
        }

        QTextStream in(&file);
        GlobalScript = in.readAll();
    }

    int     port    = Settings::internal.port.GetValue();

    QString pscript = GlobalScript.arg(port).arg(QString::fromStdString(authcode));

    return pscript;
}

void QuasarWidget::SaveSettings()
{
    auto cfglock = config.lock();
    auto qname   = QString::fromStdString(name);
    cfglock->WriteGeometry(qname, saveGeometry());
    cfglock->WriteWidgetSettings(qname, settings);
}

void QuasarWidget::createContextMenuActions()
{
    aName   = new QAction(QString::fromStdString(name), this);
    QFont f = aName->font();
    f.setBold(true);
    aName->setFont(f);
    connect(aName, &QAction::triggered, [&](bool e) {
        QFileInfo info(QString::fromStdString(GetFullPath()));
        QDesktopServices::openUrl(QUrl(info.absolutePath()));
    });

    aReload = new QAction(tr("&Reload"), this);
    connect(aReload, &QAction::triggered, [=, this] {
        if (webview->page()->scripts().count())
        {
            // Delete old script if it exists
            webview->page()->scripts().remove(script);

            // Insert refreshed script
            auto    authcode    = server.lock()->GenerateAuthCode();
            QString pageGlobals = GetGlobalScript(authcode);

            script.setSourceCode(pageGlobals);

            webview->page()->scripts().insert(script);
        }

        webview->reload();
    });

    aSetPos = new QAction(tr("S&et Position"), this);
    connect(aSetPos, &QAction::triggered, [&, this] {
        QDialog*     dialog = new QDialog(this);
        QFormLayout* form   = new QFormLayout(dialog);

        dialog->setWindowTitle("Set Position");

        QPoint    pos   = this->pos();

        QSpinBox* wEdit = new QSpinBox(dialog);
        wEdit->setRange(-16384, 16384);
        wEdit->setSuffix("px");
        wEdit->setValue(pos.x());
        QString wLabel = QString("X");
        form->addRow(wLabel, wEdit);

        QSpinBox* hEdit = new QSpinBox(dialog);
        hEdit->setRange(-16384, 16384);
        hEdit->setSuffix("px");
        hEdit->setValue(pos.y());
        QString hLabel = QString("Y");
        form->addRow(hLabel, hEdit);

        QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, dialog);
        form->addRow(buttonBox);

        connect(buttonBox, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
        connect(buttonBox, &QDialogButtonBox::rejected, dialog, &QDialog::reject);

        connect(dialog, &QDialog::finished, [=, this](int result) {
            if (result == QDialog::Accepted)
            {
                this->move(wEdit->value(), hEdit->value());
                SaveSettings();
            }

            dialog->deleteLater();
        });

        // Show the dialog as modal
        dialog->open();
    });

    aResetPos = new QAction(tr("Reset &Position"), this);
    connect(aResetPos, &QAction::triggered, [&, this] {
        this->move(0, 0);
        SaveSettings();
    });

    aOnTop = new QAction(tr("&Always on Top"), this);
    aOnTop->setCheckable(true);
    connect(aOnTop, &QAction::triggered, this, &QuasarWidget::toggleOnTop);

    aFixedPos = new QAction(tr("&Fixed Position"), this);
    aFixedPos->setCheckable(true);
    connect(aFixedPos, &QAction::triggered, [&, this](bool enabled) {
        settings.fixedPosition = enabled;
        SaveSettings();
    });

    aClickable = new QAction(tr("&Clickable"), this);
    aClickable->setCheckable(true);
    connect(aClickable, &QAction::triggered, [&, this](bool enabled) {
        overlay->setVisible(!enabled);

        settings.clickable = enabled;
        SaveSettings();
    });

    aResize = new QAction(tr("Custom &Size"), this);
    connect(aResize, &QAction::triggered, [&, this] {
        QDialog*     dialog = new QDialog(this);
        QFormLayout* form   = new QFormLayout(dialog);

        dialog->setWindowTitle("Custom Size");

        form->addRow(new QLabel("Warning: Setting a custom size may break the widget's styling!"));

        QSpinBox* wEdit = new QSpinBox(dialog);
        wEdit->setRange(1, 8192);
        wEdit->setSuffix("px");
        wEdit->setValue(size().width());
        QString wLabel = QString("Width (default %1px)").arg(settings.defaultSize.width());
        form->addRow(wLabel, wEdit);

        QSpinBox* hEdit = new QSpinBox(dialog);
        hEdit->setRange(1, 8192);
        hEdit->setSuffix("px");
        hEdit->setValue(size().height());
        QString hLabel = QString("Height (default %1px)").arg(settings.defaultSize.height());
        form->addRow(hLabel, hEdit);

        QDialogButtonBox* buttonBox =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults, Qt::Horizontal, dialog);
        form->addRow(buttonBox);

        connect(buttonBox, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
        connect(buttonBox, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
        connect(buttonBox, &QDialogButtonBox::clicked, [=, this](QAbstractButton* button) {
            auto role = buttonBox->buttonRole(button);
            if (role == QDialogButtonBox::ResetRole)
            {
                wEdit->setValue(settings.defaultSize.width());
                hEdit->setValue(settings.defaultSize.height());
            }
        });

        connect(dialog, &QDialog::finished, [=, this](int result) {
            if (result == QDialog::Accepted)
            {
                QSize nsize = {wEdit->value(), hEdit->value()};

                if (nsize != size())
                {
                    webview->resize(nsize);
                    resize(nsize);

                    settings.customSize = nsize;
                }

                SaveSettings();
            }

            dialog->deleteLater();
        });

        // Show the dialog as modal
        dialog->open();
    });

    aClose = new QAction(tr("&Close"), this);
    connect(aClose, &QAction::triggered, [&] {
        manager.lock()->CloseWidget(this);

        close();
    });
}

void QuasarWidget::createContextMenu()
{
    contextMenu = new QMenu(QString::fromStdString(name), this);
    contextMenu->addAction(aName);
    contextMenu->addSeparator();
    contextMenu->addAction(aReload);
    contextMenu->addAction(aSetPos);
    contextMenu->addAction(aResetPos);
    contextMenu->addAction(aResize);
    contextMenu->addSeparator();
    contextMenu->addAction(aOnTop);
    contextMenu->addAction(aFixedPos);
    contextMenu->addAction(aClickable);
    contextMenu->addSeparator();
    contextMenu->addAction(aClose);
}

void QuasarWidget::mousePressEvent(QMouseEvent* evt)
{
    if (!settings.fixedPosition and evt->button() == Qt::LeftButton)
    {
        dragPosition = evt->globalPosition().toPoint() - frameGeometry().topLeft();
        evt->accept();
    }
}

void QuasarWidget::mouseMoveEvent(QMouseEvent* evt)
{
    if (!settings.fixedPosition and evt->buttons() & Qt::LeftButton)
    {
        move(evt->globalPosition().toPoint() - dragPosition);
        evt->accept();
    }
}

void QuasarWidget::toggleOnTop(bool ontop)
{
    auto flags = windowFlags();

    if (ontop)
    {
        flags &= ~Qt::WindowStaysOnBottomHint;
        flags |= Qt::WindowStaysOnTopHint;
    }
    else
    {
        flags &= ~Qt::WindowStaysOnTopHint;
        flags |= Qt::WindowStaysOnBottomHint;
    }

    setWindowFlags(flags);

    // Refresh widget
    QPoint pos = this->pos();
    move(pos);
    show();

    settings.alwaysOnTop = ontop;
    SaveSettings();
}
