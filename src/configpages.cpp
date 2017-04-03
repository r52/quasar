#include <QtWidgets>

#include "quasar.h"
#include "dataplugin.h"
#include "configpages.h"
#include "widgetdefs.h"

namespace
{
    // setting names
    QString QUASAR_SETTING_PORT = "portSpin";
}

ConfigurationPage::ConfigurationPage(QObject *quasar, QWidget *parent) :
    PageWidget(parent), m_quasar(qobject_cast<Quasar*>(quasar))
{
    if (nullptr == m_quasar)
    {
        throw std::invalid_argument("Quasar window required");
    }

    // general group
    QGroupBox *configGroup = new QGroupBox(tr("General"));

    QLabel *portLabel = new QLabel(tr("Data Server port:"));

    QSettings settings;
    int port = settings.value(QUASAR_CONFIG_PORT, QUASAR_DATA_SERVER_DEFAULT_PORT).toInt();

    QSpinBox *portSpin = new QSpinBox;
    portSpin->setObjectName(QUASAR_SETTING_PORT);
    portSpin->setMinimum(1024);
    portSpin->setMaximum(65535);
    portSpin->setSingleStep(1);
    portSpin->setValue(port);

    connect(portSpin, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [this](int i) { this->m_settingsModified = true; });

    QHBoxLayout *generalLayout = new QHBoxLayout;
    generalLayout->addWidget(portLabel);
    generalLayout->addWidget(portSpin);

    QVBoxLayout *configLayout = new QVBoxLayout;
    configLayout->addLayout(generalLayout);
    configGroup->setLayout(configLayout);

    // plugin group
    QGroupBox *pluginGroup = new QGroupBox(tr("Plugins"));

    // loaded plugins
    QGroupBox *loadedGroup = new QGroupBox(tr("Loaded Plugins"));

    QListWidget *pluginList = new QListWidget;

    // build plugin list
    DataPluginMapType& pluginmap = m_quasar->getDataServer().getPlugins();

    for (DataPlugin* plugin : pluginmap)
    {
        QListWidgetItem *item = new QListWidgetItem(pluginList);
        item->setText(plugin->getName());
        item->setData(Qt::UserRole, QVariant::fromValue(plugin));
    }

    connect(pluginList, &QListWidget::itemClicked, this, &ConfigurationPage::pluginListClicked);

    QVBoxLayout *loadedLayout = new QVBoxLayout;
    loadedLayout->addWidget(pluginList);
    loadedGroup->setLayout(loadedLayout);
    loadedGroup->setFixedWidth(200);

    // plugin info
    QGroupBox *infoGroup = new QGroupBox(tr("Plugin Info"));

    // name
    QLabel *plugNameLabel = new QLabel(tr("Plugin Name:"));
    plugNameLabel->setStyleSheet("font-weight: bold");
    plugName = new QLabel;

    QHBoxLayout *plugNameLayout = new QHBoxLayout;
    plugNameLayout->setAlignment(Qt::AlignLeft);
    plugNameLayout->addWidget(plugNameLabel);
    plugNameLayout->addWidget(plugName);

    // code
    QLabel *plugCodeLabel = new QLabel(tr("Plugin Code:"));
    plugCodeLabel->setStyleSheet("font-weight: bold");
    plugCode = new QLabel;

    QHBoxLayout *plugCodeLayout = new QHBoxLayout;
    plugCodeLayout->setAlignment(Qt::AlignLeft);
    plugCodeLayout->addWidget(plugCodeLabel);
    plugCodeLayout->addWidget(plugCode);

    // version
    QLabel *plugVerLabel = new QLabel(tr("Version:"));
    plugVerLabel->setStyleSheet("font-weight: bold");
    plugVersion = new QLabel;

    QHBoxLayout *plugVerLayout = new QHBoxLayout;
    plugVerLayout->setAlignment(Qt::AlignLeft);
    plugVerLayout->addWidget(plugVerLabel);
    plugVerLayout->addWidget(plugVersion);

    // author
    QLabel *plugAuthorLabel = new QLabel(tr("Author:"));
    plugAuthorLabel->setStyleSheet("font-weight: bold");
    plugAuthor = new QLabel;

    QHBoxLayout *plugAuthorLayout = new QHBoxLayout;
    plugAuthorLayout->setAlignment(Qt::AlignLeft);
    plugAuthorLayout->addWidget(plugAuthorLabel);
    plugAuthorLayout->addWidget(plugAuthor);

    // desc
    QLabel *plugDescLabel = new QLabel(tr("Description:"));
    plugDescLabel->setStyleSheet("font-weight: bold");
    plugDesc = new QLabel;
    plugDesc->setWordWrap(true);

    QHBoxLayout *plugDescLayout = new QHBoxLayout;
    plugDescLayout->setAlignment(Qt::AlignLeft);
    plugDescLayout->addWidget(plugDescLabel);
    plugDescLayout->addWidget(plugDesc);

    // layout
    QVBoxLayout *infoLayout = new QVBoxLayout;
    infoLayout->addLayout(plugNameLayout);
    infoLayout->addLayout(plugCodeLayout);
    infoLayout->addLayout(plugVerLayout);
    infoLayout->addLayout(plugAuthorLayout);
    infoLayout->addLayout(plugDescLayout);
    infoGroup->setLayout(infoLayout);

    // plugin group layout
    QHBoxLayout *pluginLayout = new QHBoxLayout;
    pluginLayout->addWidget(loadedGroup);
    pluginLayout->addWidget(infoGroup);

    pluginGroup->setLayout(pluginLayout);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(configGroup);
    mainLayout->addWidget(pluginGroup);
    mainLayout->addStretch(1);
    setLayout(mainLayout);
}

void ConfigurationPage::saveSettings(QSettings &settings, bool &restartNeeded)
{
    if (m_settingsModified)
    {
        auto portspin = findChild<QSpinBox *>(QUASAR_SETTING_PORT);

        if (portspin)
        {
            settings.setValue(QUASAR_CONFIG_PORT, portspin->value());
            restartNeeded = true;
        }
    }
}

void ConfigurationPage::pluginListClicked(QListWidgetItem *item)
{
    DataPlugin *plugin = qvariant_cast<DataPlugin*>(item->data(Qt::UserRole));

    if (plugin)
    {
        plugName->setText(plugin->getName());
        plugCode->setText(plugin->getCode());
        plugVersion->setText(plugin->getVersion());
        plugAuthor->setText(plugin->getAuthor());
        plugDesc->setText(plugin->getDesc());
    }
}

PluginPage::PluginPage(QObject* quasar, QWidget *parent) :
    PageWidget(parent), m_quasar(qobject_cast<Quasar*>(quasar))
{
    if (nullptr == m_quasar)
    {
        throw std::invalid_argument("Quasar window required");
    }

    QLabel *pluginLabel = new QLabel(tr("Plugin:"));
    QComboBox *pluginCombo = new QComboBox;
    pagesWidget = new QStackedWidget;

    // build plugin list
    DataPluginMapType& pluginmap = m_quasar->getDataServer().getPlugins();

    for (DataPlugin* plugin : pluginmap)
    {
        pluginCombo->addItem(plugin->getName(), QVariant::fromValue(plugin));
        pagesWidget->addWidget(new DataPluginPage(plugin));
    }

    QHBoxLayout *pluginLayout = new QHBoxLayout;
    pluginLayout->addWidget(pluginLabel);
    pluginLayout->addWidget(pluginCombo);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addLayout(pluginLayout);
    mainLayout->addWidget(pagesWidget);
    mainLayout->addSpacing(12);
    mainLayout->addStretch(1);
    setLayout(mainLayout);
}

void PluginPage::saveSettings(QSettings &settings, bool &restartNeeded)
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

DataPluginPage::DataPluginPage(DataPlugin* plugin, QWidget *parent) :
    PageWidget(parent)
{
    QGroupBox *sourceGroup = new QGroupBox(tr("Data Update Rate"));
    QVBoxLayout *sourceLayout = new QVBoxLayout;

    DataSourceMapType& sources = plugin->getDataSources();

    auto it = sources.begin();

    while (it != sources.end())
    {
        // do stuff
        QCheckBox *sourceCheckBox = new QCheckBox(it.key());
        sourceCheckBox->setChecked(true);

        QSpinBox *upSpin = new QSpinBox;
        upSpin->setMinimum(0);
        upSpin->setMaximum(INT_MAX);
        upSpin->setSingleStep(1);
        upSpin->setValue(it.value().refreshmsec);
        upSpin->setSuffix("ms");

        QHBoxLayout *dataLayout = new QHBoxLayout;
        dataLayout->addWidget(sourceCheckBox);
        dataLayout->addWidget(upSpin);

        sourceLayout->addLayout(dataLayout);

        ++it;
    }

    sourceGroup->setLayout(sourceLayout);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(sourceGroup);
    setLayout(mainLayout);
}

void DataPluginPage::saveSettings(QSettings &settings, bool &restartNeeded)
{
    // TODO
}
