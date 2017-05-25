#include "configdialog.h"

#include "configpages.h"

#include <QtWidgets>

ConfigDialog::ConfigDialog(QObject* quasar)
{
    contentsWidget = new QListWidget;
    contentsWidget->setViewMode(QListView::IconMode);
    contentsWidget->setIconSize(QSize(96, 84));
    contentsWidget->setMovement(QListView::Static);
    contentsWidget->setMaximumWidth(128);
    contentsWidget->setMinimumWidth(128);
    contentsWidget->setSpacing(12);

    pagesWidget = new QStackedWidget;
    pagesWidget->addWidget(new GeneralPage(quasar));
    pagesWidget->addWidget(new PluginPage(quasar));
    pagesWidget->addWidget(new LauncherPage(quasar));

    QPushButton* okButton    = new QPushButton(tr("OK"));
    QPushButton* closeButton = new QPushButton(tr("Close"));

    createIcons();
    contentsWidget->setCurrentRow(0);

    connect(okButton, &QAbstractButton::clicked, this, &ConfigDialog::saveSettings);
    connect(closeButton, &QAbstractButton::clicked, this, &QWidget::close);

    QHBoxLayout* horizontalLayout = new QHBoxLayout;
    horizontalLayout->addWidget(contentsWidget);
    horizontalLayout->addWidget(pagesWidget, 1);

    QHBoxLayout* buttonsLayout = new QHBoxLayout;
    buttonsLayout->addStretch(1);
    buttonsLayout->addWidget(okButton);
    buttonsLayout->addWidget(closeButton);

    QVBoxLayout* mainLayout = new QVBoxLayout;
    mainLayout->addLayout(horizontalLayout);
    mainLayout->addStretch(1);
    mainLayout->addSpacing(12);
    mainLayout->addLayout(buttonsLayout);
    setLayout(mainLayout);

    setWindowTitle(tr("Settings"));
    setMinimumWidth(600);
}

void ConfigDialog::createIcons()
{
    QListWidgetItem* generalButton = new QListWidgetItem(contentsWidget);
    generalButton->setText(tr("General"));
    generalButton->setTextAlignment(Qt::AlignHCenter);
    generalButton->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    QListWidgetItem* pluginButton = new QListWidgetItem(contentsWidget);
    pluginButton->setText(tr("Plugins"));
    pluginButton->setTextAlignment(Qt::AlignHCenter);
    pluginButton->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    QListWidgetItem* launcherButton = new QListWidgetItem(contentsWidget);
    launcherButton->setText(tr("Launcher"));
    launcherButton->setTextAlignment(Qt::AlignHCenter);
    launcherButton->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    connect(contentsWidget, &QListWidget::currentItemChanged, this, &ConfigDialog::changePage);
}

void ConfigDialog::changePage(QListWidgetItem* current, QListWidgetItem* previous)
{
    if (!current)
        current = previous;

    pagesWidget->setCurrentIndex(contentsWidget->row(current));
}

void ConfigDialog::saveSettings()
{
    QSettings settings;
    bool      restartNeeded = false;

    auto pages = pagesWidget->children();

    for (QObject* page : pages)
    {
        auto p = qobject_cast<PageWidget*>(page);

        if (p)
        {
            p->saveSettings(settings, restartNeeded);
        }
    }

    if (restartNeeded)
    {
        QMessageBox::information(nullptr, "Quasar", "Quasar needs to be restarted for these changes to take effect. Please restart the app.");
    }

    close();
}
