#include "configpages.h"

#include "applauncher.h"
#include "dataplugin.h"
#include "dataserver.h"
#include "dataservices.h"
#include "plugin_support_internal.h"
#include "widgetdefs.h"

#include <QtWidgets>

namespace
{
    // setting names
    QString QUASAR_SETTING_PORT    = "portSpin";
    QString QUASAR_SETTING_LOG     = "logCombo";
    QString QUASAR_SETTING_COOKIE  = "cookieEdit";
    QString QUASAR_SETTING_STARTUP = "startUp";
    QString QUASAR_SETTING_LOGFILE = "logFile";
}

GeneralPage::GeneralPage(DataServices* service, QWidget* parent)
    : PageWidget(parent), m_service(service)
{
    if (nullptr == service)
    {
        throw std::invalid_argument("DataServices required");
    }

    // general group
    QGroupBox* configGroup = new QGroupBox(tr("General"));

    QLabel* portLabel = new QLabel(tr("Data Server port:"));

    QSettings settings;

    // ------------------Port
    int port = settings.value(QUASAR_CONFIG_PORT, QUASAR_DATA_SERVER_DEFAULT_PORT).toInt();

    QSpinBox* portSpin = new QSpinBox;
    portSpin->setObjectName(QUASAR_SETTING_PORT);
    portSpin->setMinimum(1024);
    portSpin->setMaximum(65535);
    portSpin->setSingleStep(1);
    portSpin->setValue(port);

    connect(portSpin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this](int i) { this->m_settingsModified = true; });

    QHBoxLayout* generalLayout = new QHBoxLayout;
    generalLayout->addWidget(portLabel);
    generalLayout->addWidget(portSpin);

    // ------------------Log verbosity
    QLabel* logLabel = new QLabel(tr("Log Verbosity:"));

    QComboBox* logCombo = new QComboBox;
    logCombo->setObjectName(QUASAR_SETTING_LOG);
    logCombo->addItems(QStringList() << "Debug"
                                     << "Info"
                                     << "Warning"
                                     << "Critical");
    logCombo->setCurrentIndex(settings.value(QUASAR_CONFIG_LOGLEVEL, QUASAR_CONFIG_DEFAULT_LOGLEVEL).toInt());

    QHBoxLayout* logLayout = new QHBoxLayout;
    logLayout->addWidget(logLabel);
    logLayout->addWidget(logCombo);

    // ------------------Cookies.txt
    QLabel* cookieLabel = new QLabel(tr("cookies.txt:"));

    QLineEdit* cookieEdit = new QLineEdit;
    cookieEdit->setObjectName(QUASAR_SETTING_COOKIE);
    cookieEdit->setText(settings.value(QUASAR_CONFIG_COOKIES).toString());

    connect(cookieEdit, &QLineEdit::textChanged, [=] {
        this->m_settingsModified = true;
    });

    QPushButton* cookieBrowse = new QPushButton(tr("Browse"));

    connect(cookieBrowse, &QPushButton::clicked, [=] {
        QString fname = QFileDialog::getOpenFileName(this, tr("Cookie File"), QDir::currentPath(), tr("Cookie File (cookies.txt)"));
        cookieEdit->setText(fname);
    });

    QHBoxLayout* cookieLayout = new QHBoxLayout;
    cookieLayout->addWidget(cookieLabel);
    cookieLayout->addWidget(cookieEdit);
    cookieLayout->addWidget(cookieBrowse);

    // ------------------Log to file
    QCheckBox* logToFile = new QCheckBox("Save log to file");
    logToFile->setObjectName(QUASAR_SETTING_LOGFILE);
    logToFile->setChecked(settings.value(QUASAR_CONFIG_LOGFILE, false).toBool());

#ifdef WIN32
    // ------------------Startup launch
    QString   startupFolder = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation) + "/Startup";
    QFileInfo lnk(startupFolder + "/Quasar.lnk");

    QCheckBox* startOnStartup = new QCheckBox("Launch Quasar when system starts");
    startOnStartup->setObjectName(QUASAR_SETTING_STARTUP);
    startOnStartup->setChecked(lnk.exists());
#endif

    // ------------------Clear geolocation
    /*
    QPushButton* resetGeoButton = new QPushButton(tr("Reset Geolocation Permissions"));
    connect(resetGeoButton, &QPushButton::clicked, [=] {
        QMessageBox msgBox(this);
        msgBox.setText(tr("This will reset geolocation permissions for all widgets."));
        msgBox.setInformativeText(tr("Continue?"));
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msgBox.setDefaultButton(QMessageBox::Yes);

        if (msgBox.exec() == QMessageBox::Yes)
        {
            QSettings settings;
            settings.setValue(QUASAR_CONFIG_ALLOWGEO, QVariantMap());
        }
    });
    */

    QVBoxLayout* configLayout = new QVBoxLayout;
    configLayout->addLayout(generalLayout);
    configLayout->addLayout(logLayout);
    configLayout->addLayout(cookieLayout);
    configLayout->addWidget(logToFile);
#ifdef WIN32
    configLayout->addWidget(startOnStartup);
#endif
    //configLayout->addWidget(resetGeoButton);
    configGroup->setLayout(configLayout);

    // plugin group
    QGroupBox* pluginGroup = new QGroupBox(tr("Plugins"));

    // loaded plugins
    QGroupBox* loadedGroup = new QGroupBox(tr("Loaded Plugins"));

    QListWidget* pluginList = new QListWidget;

    // build plugin list
    DataPluginMapType& pluginmap = m_service->getServer()->getPlugins();

    for (auto& p : pluginmap)
    {
        QListWidgetItem* item = new QListWidgetItem(pluginList);
        item->setText(p.second->getName());
        item->setData(Qt::UserRole, QVariant::fromValue(p.second.get()));
    }

    connect(pluginList, &QListWidget::itemClicked, this, &GeneralPage::pluginListClicked);

    QVBoxLayout* loadedLayout = new QVBoxLayout;
    loadedLayout->addWidget(pluginList);
    loadedGroup->setLayout(loadedLayout);
    loadedGroup->setFixedWidth(200);

    // plugin info
    QGroupBox* infoGroup = new QGroupBox(tr("Plugin Info"));

    // name
    QLabel* plugNameLabel = new QLabel(tr("Plugin Name:"));
    plugNameLabel->setStyleSheet("font-weight: bold");
    plugName = new QLabel;

    QHBoxLayout* plugNameLayout = new QHBoxLayout;
    plugNameLayout->setAlignment(Qt::AlignLeft);
    plugNameLayout->addWidget(plugNameLabel);
    plugNameLayout->addWidget(plugName);

    // code
    QLabel* plugCodeLabel = new QLabel(tr("Plugin Code:"));
    plugCodeLabel->setStyleSheet("font-weight: bold");
    plugCode = new QLabel;

    QHBoxLayout* plugCodeLayout = new QHBoxLayout;
    plugCodeLayout->setAlignment(Qt::AlignLeft);
    plugCodeLayout->addWidget(plugCodeLabel);
    plugCodeLayout->addWidget(plugCode);

    // version
    QLabel* plugVerLabel = new QLabel(tr("Version:"));
    plugVerLabel->setStyleSheet("font-weight: bold");
    plugVersion = new QLabel;

    QHBoxLayout* plugVerLayout = new QHBoxLayout;
    plugVerLayout->setAlignment(Qt::AlignLeft);
    plugVerLayout->addWidget(plugVerLabel);
    plugVerLayout->addWidget(plugVersion);

    // author
    QLabel* plugAuthorLabel = new QLabel(tr("Author:"));
    plugAuthorLabel->setStyleSheet("font-weight: bold");
    plugAuthor = new QLabel;

    QHBoxLayout* plugAuthorLayout = new QHBoxLayout;
    plugAuthorLayout->setAlignment(Qt::AlignLeft);
    plugAuthorLayout->addWidget(plugAuthorLabel);
    plugAuthorLayout->addWidget(plugAuthor);

    // desc
    QLabel* plugDescLabel = new QLabel(tr("Description:"));
    plugDescLabel->setStyleSheet("font-weight: bold");
    plugDesc = new QLabel;
    plugDesc->setWordWrap(true);

    QHBoxLayout* plugDescLayout = new QHBoxLayout;
    plugDescLayout->setAlignment(Qt::AlignLeft);
    plugDescLayout->addWidget(plugDescLabel);
    plugDescLayout->addWidget(plugDesc);

    // layout
    QVBoxLayout* infoLayout = new QVBoxLayout;
    infoLayout->addLayout(plugNameLayout);
    infoLayout->addLayout(plugCodeLayout);
    infoLayout->addLayout(plugVerLayout);
    infoLayout->addLayout(plugAuthorLayout);
    infoLayout->addLayout(plugDescLayout);
    infoGroup->setLayout(infoLayout);

    // plugin group layout
    QHBoxLayout* pluginLayout = new QHBoxLayout;
    pluginLayout->addWidget(loadedGroup);
    pluginLayout->addWidget(infoGroup);

    pluginGroup->setLayout(pluginLayout);

    QVBoxLayout* mainLayout = new QVBoxLayout;
    mainLayout->addWidget(configGroup);
    mainLayout->addWidget(pluginGroup);
    mainLayout->addStretch(1);
    setLayout(mainLayout);
}

void GeneralPage::saveSettings(QSettings& settings, bool& restartNeeded)
{
    if (m_settingsModified)
    {
        auto portspin = findChild<QSpinBox*>(QUASAR_SETTING_PORT);

        if (portspin)
        {
            settings.setValue(QUASAR_CONFIG_PORT, portspin->value());
        }

        auto cookieEdit = findChild<QLineEdit*>(QUASAR_SETTING_COOKIE);

        if (cookieEdit)
        {
            settings.setValue(QUASAR_CONFIG_COOKIES, cookieEdit->text());
        }

        restartNeeded = true;
    }

    auto logcombo = findChild<QComboBox*>(QUASAR_SETTING_LOG);

    if (logcombo)
    {
        settings.setValue(QUASAR_CONFIG_LOGLEVEL, logcombo->currentIndex());
    }

    auto logCheck = findChild<QCheckBox*>(QUASAR_SETTING_LOGFILE);

    if (logCheck)
    {
        settings.setValue(QUASAR_CONFIG_LOGFILE, logCheck->isChecked());
    }

#ifdef WIN32
    auto startCheck = findChild<QCheckBox*>(QUASAR_SETTING_STARTUP);

    if (startCheck)
    {
        QString   startupFolder = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation) + "/Startup";
        QString   filename      = startupFolder + "/Quasar.lnk";
        QFileInfo lnk(filename);

        bool checked = startCheck->isChecked();

        if (checked && !lnk.exists())
        {
            QFile::link(QCoreApplication::applicationFilePath(), filename);
        }
        else if (!checked && lnk.exists())
        {
            QFile::remove(filename);
        }
    }
#endif
}

void GeneralPage::pluginListClicked(QListWidgetItem* item)
{
    DataPlugin* plugin = qvariant_cast<DataPlugin*>(item->data(Qt::UserRole));

    if (plugin)
    {
        plugName->setText(plugin->getName());
        plugCode->setText(plugin->getCode());
        plugVersion->setText(plugin->getVersion());
        plugAuthor->setText(plugin->getAuthor());
        plugDesc->setText(plugin->getDesc());
    }
}

PluginPage::PluginPage(DataServices* service, QWidget* parent)
    : PageWidget(parent), m_service(service)
{
    if (nullptr == m_service)
    {
        throw std::invalid_argument("DataServices required");
    }

    QLabel*    pluginLabel = new QLabel(tr("Plugins:"));
    QComboBox* pluginCombo = new QComboBox;
    pagesWidget            = new QStackedWidget;

    // build plugin list
    DataPluginMapType& pluginmap = m_service->getServer()->getPlugins();

    for (auto& p : pluginmap)
    {
        pluginCombo->addItem(p.second->getName(), QVariant::fromValue(p.second.get()));
        pagesWidget->addWidget(new DataPluginPage(p.second.get()));
    }

    connect(pluginCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), [=](int index) {
        pagesWidget->setCurrentIndex(index);
    });

    QHBoxLayout* pluginLayout = new QHBoxLayout;
    pluginLayout->addWidget(pluginLabel);
    pluginLayout->addWidget(pluginCombo);

    QVBoxLayout* mainLayout = new QVBoxLayout;
    mainLayout->addLayout(pluginLayout);
    mainLayout->addWidget(pagesWidget);
    mainLayout->addSpacing(12);
    mainLayout->addStretch(1);
    setLayout(mainLayout);
}

void PluginPage::saveSettings(QSettings& settings, bool& restartNeeded)
{
    auto pages = pagesWidget->children();

    for (QObject* page : pages)
    {
        auto p = qobject_cast<PageWidget*>(page);

        if (p)
        {
            p->saveSettings(settings, restartNeeded);
        }
    }
}

DataPluginPage::DataPluginPage(DataPlugin* p, QWidget* parent)
    : PageWidget(parent), plugin(p)
{
    QGroupBox*   sourceGroup  = new QGroupBox(tr("Data Update Rate"));
    QVBoxLayout* sourceLayout = new QVBoxLayout;

    // Create data source settings
    DataSourceMapType& sources = p->getDataSources();

    QSettings settings;

    for (auto& it : sources)
    {
        bool sourceEnabled = settings.value(plugin->getSettingsCode(QUASAR_DP_ENABLED_PREFIX + it.first), true).toBool();

        QHBoxLayout* dataLayout = new QHBoxLayout;

        // create data source rate settings
        QCheckBox* sourceCheckBox = new QCheckBox(it.first);
        sourceCheckBox->setObjectName(QUASAR_DP_ENABLED_PREFIX + it.first);
        sourceCheckBox->setChecked(sourceEnabled);

        // Signaled sources cannot be disabled
        if (it.second.refreshmsec < 0)
        {
            sourceCheckBox->setEnabled(false);
        }

        dataLayout->addWidget(sourceCheckBox);

        // Only create refresh setting if the data source is subscription based
        if (it.second.refreshmsec > 0)
        {
            QSpinBox* upSpin = new QSpinBox;
            upSpin->setObjectName(QUASAR_DP_REFRESH_PREFIX + it.first);
            upSpin->setMinimum(1);
            upSpin->setMaximum(INT_MAX);
            upSpin->setSingleStep(1);
            upSpin->setValue(it.second.refreshmsec);
            upSpin->setSuffix("ms");
            upSpin->setEnabled(sourceEnabled);

            connect(sourceCheckBox, &QCheckBox::toggled, [this, upSpin](bool state) {
                this->m_dataSettingsModified = true;
                upSpin->setEnabled(state);
            });

            connect(upSpin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this](int i) { this->m_dataSettingsModified = true; });

            dataLayout->addWidget(upSpin);
        }

        sourceLayout->addLayout(dataLayout);
    }

    sourceGroup->setLayout(sourceLayout);

    QVBoxLayout* mainLayout = new QVBoxLayout;
    mainLayout->addWidget(sourceGroup);

    // Create plugin custom settings if any
    if (auto s = p->getSettings())
    {
        QGroupBox*   plugGroup  = new QGroupBox(tr("Plugin Settings"));
        QVBoxLayout* plugLayout = new QVBoxLayout;

        for (auto& it : s->map)
        {
            QHBoxLayout* entryLayout = new QHBoxLayout;

            QLabel* label = new QLabel;
            label->setText(it.second.description);
            entryLayout->addWidget(label);

            switch (it.second.type)
            {
                case QUASAR_SETTING_ENTRY_INT:
                {
                    QSpinBox* s = new QSpinBox;
                    s->setObjectName(QUASAR_DP_CUSTOM_PREFIX + it.first);
                    s->setMinimum(it.second.inttype.min);
                    s->setMaximum(it.second.inttype.max);
                    s->setSingleStep(it.second.inttype.step);
                    s->setValue(it.second.inttype.val);

                    connect(s, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this](int i) { this->m_plugSettingsModified = true; });

                    entryLayout->addWidget(s);
                    break;
                }

                case QUASAR_SETTING_ENTRY_DOUBLE:
                {
                    QDoubleSpinBox* d = new QDoubleSpinBox;
                    d->setObjectName(QUASAR_DP_CUSTOM_PREFIX + it.first);
                    d->setMinimum(it.second.doubletype.min);
                    d->setMaximum(it.second.doubletype.max);
                    d->setSingleStep(it.second.doubletype.step);
                    d->setValue(it.second.doubletype.val);

                    connect(d, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), [this](double d) { this->m_plugSettingsModified = true; });

                    entryLayout->addWidget(d);
                    break;
                }

                case QUASAR_SETTING_ENTRY_BOOL:
                {
                    QCheckBox* b = new QCheckBox;
                    b->setObjectName(QUASAR_DP_CUSTOM_PREFIX + it.first);
                    b->setChecked(it.second.booltype.val);

                    connect(b, &QCheckBox::toggled, [this](bool state) { this->m_plugSettingsModified = true; });

                    entryLayout->addWidget(b);
                    break;
                }
            }

            plugLayout->addLayout(entryLayout);
        }

        plugGroup->setLayout(plugLayout);
        mainLayout->addWidget(plugGroup);
    }

    setLayout(mainLayout);
}

void DataPluginPage::saveSettings(QSettings& settings, bool& restartNeeded)
{
    // Save data source settings
    if (m_dataSettingsModified)
    {
        auto refreshSources = findChildren<QSpinBox*>(QRegularExpression(QString(QUASAR_DP_REFRESH_PREFIX) + ".*"));

        for (auto s : refreshSources)
        {
            QString name = s->objectName();
            name         = name.remove(QUASAR_DP_REFRESH_PREFIX);

            plugin->setDataSourceRefresh(name, s->value());
        }

        auto enabledSources = findChildren<QCheckBox*>(QRegularExpression(QString(QUASAR_DP_ENABLED_PREFIX) + ".*"));

        for (auto c : enabledSources)
        {
            QString name = c->objectName();
            name         = name.remove(QUASAR_DP_ENABLED_PREFIX);

            plugin->setDataSourceEnabled(name, c->isChecked());
        }
    }

    // Save plugin custom settings
    if (m_plugSettingsModified)
    {
        auto intSources = findChildren<QSpinBox*>(QRegularExpression(QString(QUASAR_DP_CUSTOM_PREFIX) + ".*"));

        for (auto s : intSources)
        {
            QString name = s->objectName();
            name         = name.remove(QUASAR_DP_CUSTOM_PREFIX);

            plugin->setCustomSetting(name, s->value());
        }

        auto boolSources = findChildren<QCheckBox*>(QRegularExpression(QString(QUASAR_DP_CUSTOM_PREFIX) + ".*"));

        for (auto c : boolSources)
        {
            QString name = c->objectName();
            name         = name.remove(QUASAR_DP_CUSTOM_PREFIX);

            plugin->setCustomSetting(name, c->isChecked());
        }

        auto doubleSources = findChildren<QDoubleSpinBox*>(QRegularExpression(QString(QUASAR_DP_CUSTOM_PREFIX) + ".*"));

        for (auto d : doubleSources)
        {
            QString name = d->objectName();
            name         = name.remove(QUASAR_DP_CUSTOM_PREFIX);

            plugin->setCustomSetting(name, d->value());
        }

        plugin->updatePluginSettings();
    }
}

class LauncherEditDialog : public QDialog
{
public:
    LauncherEditDialog(QString title, QWidget* parent = Q_NULLPTR, QString command = QString(), AppLauncherData data = AppLauncherData())
        : QDialog(parent)
    {
        setMinimumWidth(400);
        setWindowTitle(title);

        QLabel*    cmdLabel = new QLabel(tr("Launcher Command:"));
        QLineEdit* cmdEdit  = new QLineEdit;
        cmdEdit->setText(command);

        QHBoxLayout* cmdlayout = new QHBoxLayout;
        cmdlayout->addWidget(cmdLabel);
        cmdlayout->addWidget(cmdEdit);

        QLabel*      fileLabel = new QLabel(tr("File/Commandline:"));
        QLineEdit*   fileEdit  = new QLineEdit;
        QPushButton* fileBtn   = new QPushButton(tr("Browse"));
        fileEdit->setText(data.file);

        QHBoxLayout* filelayout = new QHBoxLayout;
        filelayout->addWidget(fileLabel);
        filelayout->addWidget(fileEdit);
        filelayout->addWidget(fileBtn);

        QLabel*    pathLabel = new QLabel(tr("Start Path:"));
        QLineEdit* pathEdit  = new QLineEdit;
        pathEdit->setText(data.startpath);

        QHBoxLayout* pathlayout = new QHBoxLayout;
        pathlayout->addWidget(pathLabel);
        pathlayout->addWidget(pathEdit);

        QLabel*    argLabel = new QLabel(tr("Arguments:"));
        QLineEdit* argEdit  = new QLineEdit;
        argEdit->setText(data.arguments);

        QHBoxLayout* arglayout = new QHBoxLayout;
        arglayout->addWidget(argLabel);
        arglayout->addWidget(argEdit);

        QPushButton* okBtn     = new QPushButton(tr("OK"));
        QPushButton* cancelBtn = new QPushButton(tr("Cancel"));

        okBtn->setDefault(true);

        QHBoxLayout* btnlayout = new QHBoxLayout;
        btnlayout->addWidget(okBtn);
        btnlayout->addWidget(cancelBtn);

        QVBoxLayout* layout = new QVBoxLayout;
        layout->addLayout(cmdlayout);
        layout->addLayout(filelayout);
        layout->addLayout(pathlayout);
        layout->addLayout(arglayout);
        layout->addLayout(btnlayout);

        // browse button
        connect(fileBtn, &QPushButton::clicked, [=](bool checked) {
            QString filename = QFileDialog::getOpenFileName(this, tr("Choose Application"), QString(), tr("Executables (*.*)"));
            if (!filename.isEmpty())
            {
                QFileInfo info(filename);

                fileEdit->setText(info.canonicalFilePath());
                pathEdit->setText(info.canonicalPath());
            }
        });

        // cancel button
        connect(cancelBtn, &QPushButton::clicked, [=](bool checked) {
            close();
        });

        // ok button
        connect(okBtn, &QPushButton::clicked, [=](bool checked) {
            // validate
            if (cmdEdit->text().isEmpty() || fileEdit->text().isEmpty())
            {
                QMessageBox::warning(this, title, "Command and File Path must be filled out.");
            }
            else
            {
                m_command        = cmdEdit->text();
                m_data.file      = fileEdit->text();
                m_data.startpath = pathEdit->text();
                m_data.arguments = argEdit->text();
                accept();
            }
        });

        setLayout(layout);
    };

    QString&         getCommand() { return m_command; };
    AppLauncherData& getData() { return m_data; };

private:
    QString         m_command;
    AppLauncherData m_data;
};

LauncherPage::LauncherPage(DataServices* service, QWidget* parent)
    : PageWidget(parent), m_service(service)
{
    if (nullptr == m_service)
    {
        throw std::invalid_argument("Quasar window required");
    }

    QLabel* label = new QLabel(tr("Registered Apps:"));

    QTableWidget* table = new QTableWidget;
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setColumnCount(3);
    table->setHorizontalHeaderLabels(QStringList() << "Command"
                                                   << "File/Commandline"
                                                   << "Arguments");
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    auto appmap = m_service->getLauncher()->getMapForRead();
    auto it     = appmap->cbegin();
    int  row    = 0;

    while (it != appmap->cend())
    {
        if (it.value().canConvert<AppLauncherData>())
        {
            auto [file, start, arg] = it.value().value<AppLauncherData>();

            table->insertRow(row);
            QTableWidgetItem* cmditem = new QTableWidgetItem(it.key());

            QTableWidgetItem* fileitem = new QTableWidgetItem(file);
            fileitem->setData(Qt::UserRole, it.value());

            QTableWidgetItem* argitem = new QTableWidgetItem(arg);

            table->setItem(row, 0, cmditem);
            table->setItem(row, 1, fileitem);
            table->setItem(row, 2, argitem);

            ++row;
        }

        ++it;
    }

    m_service->getLauncher()->releaseMap(appmap);

    QPushButton* deleteButton = new QPushButton(tr("Delete"));

    connect(deleteButton, &QPushButton::clicked, [=](bool checked) {
        int row = table->currentRow();
        if (row >= 0)
        {
            table->removeRow(table->currentRow());
        }
    });

    QPushButton* editButton = new QPushButton(tr("Edit"));

    connect(editButton, &QPushButton::clicked, [=](bool checked) {
        bool ok;
        int  row = table->currentRow();

        if (row >= 0)
        {
            QTableWidgetItem* cmditem  = table->item(row, 0);
            QTableWidgetItem* fileitem = table->item(row, 1);
            QTableWidgetItem* argitem  = table->item(row, 2);

            QString         cmd = cmditem->text();
            AppLauncherData d   = fileitem->data(Qt::UserRole).value<AppLauncherData>();

            LauncherEditDialog editdialog("Edit App", this, cmd, d);

            if (editdialog.exec() == QDialog::Accepted)
            {
                cmd = editdialog.getCommand();
                d   = editdialog.getData();

                cmditem->setText(cmd);
                fileitem->setText(d.file);
                fileitem->setData(Qt::UserRole, QVariant::fromValue(d));
                argitem->setText(d.arguments);
            }
        }
    });

    QPushButton* addButton = new QPushButton(tr("Add"));

    connect(addButton, &QPushButton::clicked, [=](bool checked) {
        LauncherEditDialog editdialog("New App", this);

        if (editdialog.exec() == QDialog::Accepted)
        {
            QString         cmd = editdialog.getCommand();
            AppLauncherData d   = editdialog.getData();

            int row = table->rowCount();
            table->insertRow(row);

            QTableWidgetItem* cmditem  = new QTableWidgetItem(cmd);
            QTableWidgetItem* fileitem = new QTableWidgetItem(d.file);
            QTableWidgetItem* argitem  = new QTableWidgetItem(d.arguments);
            fileitem->setData(Qt::UserRole, QVariant::fromValue(d));

            table->setItem(row, 0, cmditem);
            table->setItem(row, 1, fileitem);
            table->setItem(row, 2, argitem);
        }
    });

    QHBoxLayout* buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(deleteButton);
    buttonLayout->addWidget(editButton);
    buttonLayout->addWidget(addButton);

    QVBoxLayout* layout = new QVBoxLayout;
    layout->addWidget(label);
    layout->addWidget(table);
    layout->addLayout(buttonLayout);
    layout->addStretch(1);
    setLayout(layout);
}

void LauncherPage::saveSettings(QSettings& settings, bool& restartNeeded)
{
    auto table = findChild<QTableWidget*>();

    if (table)
    {
        QVariantMap newmap;

        int rows = table->rowCount();

        for (int i = 0; i < rows; i++)
        {
            QTableWidgetItem* cmditem  = table->item(i, 0);
            QTableWidgetItem* fileitem = table->item(i, 1);

            newmap[cmditem->text()] = fileitem->data(Qt::UserRole);
        }

        m_service->getLauncher()->writeMap(newmap);
    }
}
