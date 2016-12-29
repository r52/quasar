#include "quasar.h"

#include "webwidget.h"
#include "widgetdefs.h"

#include <QVBoxLayout>
#include <QTextEdit>
#include <QMenu>
#include <QCloseEvent>
#include <QFileDialog>
#include <QDebug>
#include <QJsonDocument>
#include <QSetIterator>

Quasar::Quasar(QWidget *parent)
    : QMainWindow(parent)
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

    logEdit = new QTextEdit();
    logEdit->setReadOnly(true);
    logEdit->setAcceptRichText(true);

    layout->addWidget(logEdit);

    ui.centralWidget->setLayout(layout);

    resize(500, 400);

    // Load settings
    QSettings settings(STATE_CFG_FILENAME, QSettings::IniFormat);
    loadedList = settings.value("global/loaded").toStringList();

    foreach(const QString &f, loadedList)
    {
        loadWebWidget(f);
    }
}

Quasar::~Quasar()
{
    QSetIterator<WebWidget*> it(webwidgets);

    while (it.hasNext())
    {
        auto widget = it.next();
        widget->saveSettings();
        delete widget;
    }

    webwidgets.clear();
}

void Quasar::openWebWidget()
{
    QString fname = QFileDialog::getOpenFileName(this, tr("Load Widget"),
        QDir::currentPath(),
        tr("Widget Definitions (*.json)"));

    if (!loadedList.contains(fname))
    {
        if (loadWebWidget(fname))
        {
            loadedList.append(fname);
            QSettings settings(STATE_CFG_FILENAME, QSettings::IniFormat);
            settings.setValue("global/loaded", loadedList);
        }
    }
}

bool Quasar::loadWebWidget(QString filename)
{
    if (!filename.isNull())
    {
        QFile wgtFile(filename);

        if (!wgtFile.open(QIODevice::ReadOnly))
        {
            qWarning() << "Failed to load '" << filename << "'";
            return false;
        }

        QByteArray wgtDat = wgtFile.readAll();
        QJsonDocument loadDoc(QJsonDocument::fromJson(wgtDat));

        QJsonObject dat = loadDoc.object();
        dat[WGT_DEF_FULLPATH] = filename;

        if (WebWidget::validateWidgetDefinition(dat))
        {
            qDebug() << "Loading widget " << dat[WGT_DEF_FULLPATH].toString();
            WebWidget *widget = new WebWidget(dat);
            webwidgets.insert(widget);

            connect(widget, &WebWidget::WebWidgetClosed, this, &Quasar::closeWebWidget);
            widget->show();

            return true;
        }
        else
        {
            qWarning() << "Invalid widget definition '" << filename << "'";
        }
    }
    else
    {
        qWarning() << "Null filename";
    }

    return false;
}

void Quasar::closeWebWidget(WebWidget* widget)
{
    // Remove from loaded list
    QJsonObject data = widget->getData();
    loadedList.removeOne(data[WGT_DEF_FULLPATH].toString());

    // Remove from registry
    auto it = webwidgets.find(widget);

    if (it != webwidgets.end())
    {
        webwidgets.erase(it);
    }

    widget->deleteLater();
}

void Quasar::createTrayIcon()
{
    trayIconMenu = new QMenu(this);
    trayIconMenu->addAction(loadAction);
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
