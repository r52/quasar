#include "settingsdialog.h"

#include "webuihandler.h"
#include <QtWebEngineWidgets/QWebEngineView>

SettingsDialog::SettingsDialog(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowModality(Qt::ApplicationModal);

    profile = new QWebEngineProfile;

    WebUiHandler* handler = new WebUiHandler(profile);
    profile->installUrlSchemeHandler(WebUiHandler::schemeName, handler);

    QWebEnginePage* page = new QWebEnginePage(profile, this);
    page->load(WebUiHandler::settingsUrl);

    QWebEngineView* view = new QWebEngineView(this);
    view->setPage(page);
    view->setContextMenuPolicy(Qt::NoContextMenu);
    view->resize(1000, 600);

    connect(page, &QWebEnginePage::windowCloseRequested, [=] {
        this->close();
    });

    resize(1000, 600);
}

SettingsDialog::~SettingsDialog()
{
    profile->deleteLater();
}
