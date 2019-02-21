#include "webuidialog.h"

#include "dataserver.h"
#include "webuihandler.h"
#include "webwidget.h"
#include "widgetdefs.h"

#include <QFile>
#include <QSettings>
#include <QVBoxLayout>
#include <QtWebEngineWidgets/QWebEngineScript>
#include <QtWebEngineWidgets/QWebEngineScriptCollection>
#include <QtWebEngineWidgets/QWebEngineView>

WebUiDialog::WebUiDialog(DataServer* server, QString title, QUrl url, ClientAccessLevel lvl, QSize size, QWidget* parent) : QWidget(parent)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowModality(Qt::WindowModal);

    profile = new QWebEngineProfile;

    WebUiHandler* handler = new WebUiHandler(profile);
    profile->installUrlSchemeHandler(WebUiHandler::schemeName, handler);

    QuasarWebPage* page = new QuasarWebPage(profile, this);
    page->load(url);

    QWebEngineView* view = new QWebEngineView(this);
    view->setPage(page);
    view->setContextMenuPolicy(Qt::NoContextMenu);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(view);
    setLayout(layout);

    connect(page, &QWebEnginePage::windowCloseRequested, [=] { this->close(); });

    QString authcode = server->generateAuthCode(WebUiHandler::settingsUrl.toString(), lvl);

    QString gscript = WebWidget::getGlobalScript();

    QSettings settings;
    quint16   port = settings.value(QUASAR_CONFIG_PORT, QUASAR_DATA_SERVER_DEFAULT_PORT).toUInt();

    QString pageGlobals = gscript.arg(port).arg(authcode);

    QWebEngineScript script;
    script.setName("PageGlobals");
    script.setInjectionPoint(QWebEngineScript::DocumentCreation);
    script.setWorldId(0);
    script.setSourceCode(pageGlobals);

    view->page()->scripts().insert(script);

    setWindowTitle(title);
    resize(size);
}

WebUiDialog::~WebUiDialog()
{
    profile->deleteLater();
}
