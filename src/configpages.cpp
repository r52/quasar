#include <QtWidgets>

#include "quasar.h"
#include "dataplugin.h"
#include "configpages.h"
#include "widgetdefs.h"

#include "plugin_support_internal.h"

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

DataPluginPage::DataPluginPage(DataPlugin* p, QWidget *parent) :
    PageWidget(parent), plugin(p)
{
    QGroupBox *sourceGroup = new QGroupBox(tr("Data Update Rate"));
    QVBoxLayout *sourceLayout = new QVBoxLayout;

    // Create data source settings
    DataSourceMapType& sources = p->getDataSources();

    QSettings settings;

    auto it = sources.begin();

    while (it != sources.end())
    {
        bool sourceEnabled = settings.value(plugin->getSettingsCode(QUASAR_DP_ENABLED_PREFIX + it.key()), true).toBool();

        // create data source rate settings
        QCheckBox *sourceCheckBox = new QCheckBox(it.key());
        sourceCheckBox->setObjectName(QUASAR_DP_ENABLED_PREFIX + it.key());
        sourceCheckBox->setChecked(sourceEnabled);

        QSpinBox *upSpin = new QSpinBox;
        upSpin->setObjectName(QUASAR_DP_REFRESH_PREFIX + it.key());
        upSpin->setMinimum(0);
        upSpin->setMaximum(INT_MAX);
        upSpin->setSingleStep(1);
        upSpin->setValue(it.value().refreshmsec);
        upSpin->setSuffix("ms");
        upSpin->setEnabled(sourceEnabled);

        connect(sourceCheckBox, &QCheckBox::toggled, upSpin, [this, upSpin](bool state)
        {
            this->m_dataSettingsModified = true;
            upSpin->setEnabled(state);
        });

        connect(upSpin, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [this](int i) { this->m_dataSettingsModified = true; });

        QHBoxLayout *dataLayout = new QHBoxLayout;
        dataLayout->addWidget(sourceCheckBox);
        dataLayout->addWidget(upSpin);

        sourceLayout->addLayout(dataLayout);

        ++it;
    }

    sourceGroup->setLayout(sourceLayout);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(sourceGroup);

    // Create plugin custom settings if any
    if (auto s = p->getSettings())
    {
        QGroupBox *plugGroup = new QGroupBox(tr("Plugin Settings"));
        QVBoxLayout *plugLayout = new QVBoxLayout;

        auto it = s->map.begin();

        while (it != s->map.end())
        {
            QHBoxLayout *entryLayout = new QHBoxLayout;

            QLabel *label = new QLabel;
            label->setText(it->description);
            entryLayout->addWidget(label);

            switch (it->type)
            {
                case QUASAR_SETTING_ENTRY_INT:
                {
                    QSpinBox *s = new QSpinBox;
                    s->setObjectName(QUASAR_DP_CUSTOM_PREFIX + it.key());
                    s->setMinimum(it->inttype.min);
                    s->setMaximum(it->inttype.max);
                    s->setSingleStep(it->inttype.step);
                    s->setValue(it->inttype.val);

                    connect(s, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [this](int i) { this->m_plugSettingsModified = true; });

                    entryLayout->addWidget(s);
                    break;
                }

                case QUASAR_SETTING_ENTRY_DOUBLE:
                {
                    QDoubleSpinBox *d = new QDoubleSpinBox;
                    d->setObjectName(QUASAR_DP_CUSTOM_PREFIX + it.key());
                    d->setMinimum(it->doubletype.min);
                    d->setMaximum(it->doubletype.max);
                    d->setSingleStep(it->doubletype.step);
                    d->setValue(it->doubletype.val);

                    connect(d, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, [this](double d) { this->m_plugSettingsModified = true; });

                    entryLayout->addWidget(d);
                    break;
                }

                case QUASAR_SETTING_ENTRY_BOOL:
                {
                    QCheckBox *b = new QCheckBox;
                    b->setObjectName(QUASAR_DP_CUSTOM_PREFIX + it.key());
                    b->setChecked(it->booltype.val);

                    connect(b, &QCheckBox::toggled, this, [this](bool state) { this->m_plugSettingsModified = true; });

                    entryLayout->addWidget(b);
                    break;
                }
            }

            plugLayout->addLayout(entryLayout);

            ++it;
        }

        plugGroup->setLayout(plugLayout);
        mainLayout->addWidget(plugGroup);
    }

    setLayout(mainLayout);
}

void DataPluginPage::saveSettings(QSettings &settings, bool &restartNeeded)
{
    // Save data source settings
    if (m_dataSettingsModified)
    {
        QList<QSpinBox*> refreshSources = findChildren<QSpinBox*>(QRegularExpression(QString(QUASAR_DP_REFRESH_PREFIX) + ".*"));

        for (QSpinBox* s : refreshSources)
        {
            QString name = s->objectName();
            name = name.remove(QUASAR_DP_REFRESH_PREFIX);

            plugin->setDataSourceRefresh(name, s->value());
        }

        QList<QCheckBox*> enabledSources = findChildren<QCheckBox*>(QRegularExpression(QString(QUASAR_DP_ENABLED_PREFIX) + ".*"));

        for (QCheckBox* c : enabledSources)
        {
            QString name = c->objectName();
            name = name.remove(QUASAR_DP_ENABLED_PREFIX);

            plugin->setDataSourceEnabled(name, c->isChecked());
        }
    }

    // Save plugin custom settings
    if (m_plugSettingsModified)
    {
        QList<QSpinBox*> intSources = findChildren<QSpinBox*>(QRegularExpression(QString(QUASAR_DP_CUSTOM_PREFIX) + ".*"));

        for (QSpinBox* s : intSources)
        {
            QString name = s->objectName();
            name = name.remove(QUASAR_DP_CUSTOM_PREFIX);

            plugin->setCustomSetting(name, s->value());
        }

        QList<QCheckBox*> boolSources = findChildren<QCheckBox*>(QRegularExpression(QString(QUASAR_DP_CUSTOM_PREFIX) + ".*"));

        for (QCheckBox* c : boolSources)
        {
            QString name = c->objectName();
            name = name.remove(QUASAR_DP_CUSTOM_PREFIX);

            plugin->setCustomSetting(name, c->isChecked());
        }

        QList<QDoubleSpinBox*> doubleSources = findChildren<QDoubleSpinBox*>(QRegularExpression(QString(QUASAR_DP_CUSTOM_PREFIX) + ".*"));

        for (QDoubleSpinBox* d : doubleSources)
        {
            QString name = d->objectName();
            name = name.remove(QUASAR_DP_CUSTOM_PREFIX);

            plugin->setCustomSetting(name, d->value());
        }

        plugin->updatePluginSettings();
    }
}
