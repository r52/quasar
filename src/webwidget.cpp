#include "webwidget.h"

#include "dataserver.h"
#include "widgetdefs.h"

#include <QAction>
#include <QMenu>
#include <QMessageBox>
#include <QtWebEngineWidgets/QWebEngineCertificateError>
#include <QtWebEngineWidgets/QWebEngineScriptCollection>
#include <QtWebEngineWidgets/QWebEngineSettings>

QString WebWidget::PageGlobalScript;

void QuasarWebPage::javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level, const QString& message, int lineNumber, const QString& sourceID)
{
    QString msg = "CONSOLE: " + message + " (" + sourceID + ":" + QString::number(lineNumber) + ")";

    switch (level)
    {
        case InfoMessageLevel:
            qInfo().noquote() << msg;
            break;
        case WarningMessageLevel:
            qWarning().noquote() << msg;
            break;
        case ErrorMessageLevel:
            qCritical().noquote() << msg;
            break;
    }
}

bool QuasarWebPage::certificateError(const QWebEngineCertificateError& certificateError)
{
    auto url  = certificateError.url();
    auto host = url.host();

    if (certificateError.error() == QWebEngineCertificateError::CertificateAuthorityInvalid && url.scheme() == "wss" &&
        (host == "localhost" || host == "::1" || host == "127.0.0.1"))
    {
        // Ignore invalid CA for now for localhost due to custom localhost cert and
        // QWebEngine having no support for local certs
        return true;
    }

    qWarning().nospace() << "QWebEngineCertificateError(" << (int) certificateError.error() << "): " << certificateError.errorDescription() << " " << url;
    return false;
}

WebWidget::WebWidget(QString widgetName, const QJsonObject& dat, DataServer* serv, QWidget* parent) : QWidget(parent), m_Name(widgetName), server(serv)
{
    if (m_Name.isEmpty())
    {
        throw std::invalid_argument("Widget name cannot be null");
    }

    if (server == nullptr)
    {
        throw std::invalid_argument("Dataserver cannot be null");
    }

    // Copy data
    data = dat;

    // No frame/border, no taskbar button
    Qt::WindowFlags flags = Qt::FramelessWindowHint | Qt::Tool;

    webview = new QuasarWebView(this);

    QString entryPath = data[WGT_DEF_STARTFILE].toString();
    QUrl    startFile;

    if (entryPath.startsWith("http"))
    {
        // if url, take as is
        startFile = entryPath;
    }
    else
    {
        // assume local file
        QString startFilePath = QFileInfo(data[WGT_DEF_FULLPATH].toString()).canonicalPath().append("/");
        startFile             = QUrl::fromLocalFile(startFilePath.append(entryPath));
    }

    QuasarWebPage* page = new QuasarWebPage(this);
    page->load(startFile);
    webview->setPage(page);

    webview->settings()->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, false);

    // Remote access permission
    if (data[WGT_DEF_REMOTEACCESS].toBool())
    {
        webview->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
    }

    // Optional background transparency
    if (data[WGT_DEF_TRANSPARENTBG].toBool())
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
    QSettings settings;
    restoreGeometry(settings.value(getSettingKey("geometry")).toByteArray());
    bool ontop      = settings.value(getSettingKey("alwaysOnTop")).toBool();
    m_fixedposition = settings.value(getSettingKey("fixedPosition")).toBool();
    bool clickable  = settings.value(getSettingKey("clickable"), data[WGT_DEF_CLICKABLE].toBool()).toBool();

    rFixedPos->setChecked(m_fixedposition);
    rClickable->setChecked(clickable);

    overlay->setVisible(!clickable);

    if (ontop)
    {
        flags |= Qt::WindowStaysOnTopHint;
        rOnTop->setChecked(true);
    }
    else
    {
        flags |= Qt::WindowStaysOnBottomHint;
    }

    // Custom context menu
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, [=](const QPoint& pos) { m_Menu->exec(mapToGlobal(pos)); });

    // Set flags
    setWindowFlags(flags);

    // resize
    webview->resize(data[WGT_DEF_WIDTH].toInt(), data[WGT_DEF_HEIGHT].toInt());
    resize(data[WGT_DEF_WIDTH].toInt(), data[WGT_DEF_HEIGHT].toInt());

    // Inject global script
    if (data[WGT_DEF_DATASERVER].toBool())
    {
        QString authcode = server->generateAuthCode(m_Name);

        QString pageGlobals = getGlobalScript(authcode);

        script.setName("PageGlobals");
        script.setInjectionPoint(QWebEngineScript::DocumentCreation);
        script.setWorldId(0);
        script.setSourceCode(pageGlobals);

        webview->page()->scripts().insert(script);
    }

    setWindowTitle(m_Name);
}

WebWidget::~WebWidget()
{
    saveSettings();
}

bool WebWidget::validateWidgetDefinition(const QJsonObject& dat)
{
    if (dat.isEmpty())
    {
        qWarning() << "Empty widget definition";
        return false;
    }

    static QString errmsg = "%1 '%2' definition";

    if (!dat.contains(WGT_DEF_NAME))
    {
        qWarning().noquote() << errmsg.arg("Missing", WGT_DEF_NAME);
        return false;
    }

    if (dat[WGT_DEF_NAME].toString().isNull())
    {
        qWarning().noquote() << errmsg.arg("Invalid", WGT_DEF_NAME);
        return false;
    }

    if (!dat.contains(WGT_DEF_WIDTH))
    {
        qWarning().noquote() << errmsg.arg("Missing", WGT_DEF_WIDTH);
        return false;
    }

    if (dat[WGT_DEF_WIDTH].toInt() <= 0)
    {
        qWarning().noquote() << errmsg.arg("Invalid", WGT_DEF_WIDTH);
        return false;
    }

    if (!dat.contains(WGT_DEF_HEIGHT))
    {
        qWarning().noquote() << errmsg.arg("Missing", WGT_DEF_HEIGHT);
        return false;
    }

    if (dat[WGT_DEF_HEIGHT].toInt() <= 0)
    {
        qWarning().noquote() << errmsg.arg("Invalid", WGT_DEF_HEIGHT);
        return false;
    }

    if (!dat.contains(WGT_DEF_STARTFILE))
    {
        qWarning().noquote() << errmsg.arg("Missing", WGT_DEF_STARTFILE);
        return false;
    }

    if (dat[WGT_DEF_STARTFILE].toString().isNull())
    {
        qWarning().noquote() << errmsg.arg("Invalid", WGT_DEF_STARTFILE);
        return false;
    }

    if (!dat.contains(WGT_DEF_FULLPATH))
    {
        qWarning().noquote() << errmsg.arg("Missing", WGT_DEF_FULLPATH);
        return false;
    }

    if (dat[WGT_DEF_FULLPATH].toString().isNull())
    {
        qWarning() << errmsg.arg("Invalid", WGT_DEF_FULLPATH);
        return false;
    }

    if (dat.contains(WGT_DEF_DATASERVER) && !dat[WGT_DEF_DATASERVER].isBool())
    {
        qWarning().noquote() << errmsg.arg("Invalid", WGT_DEF_DATASERVER);
        return false;
    }

    return true;
}

bool WebWidget::acceptSecurityWarnings(const QJsonObject& dat)
{
    bool accept = true;

    if (dat.contains(WGT_DEF_REMOTEACCESS) && dat[WGT_DEF_REMOTEACCESS].toBool())
    {
        accept = false;

        auto reply = QMessageBox::warning(nullptr,
                                          tr("Remote Access"),
                                          tr("This widget requires remote access to external URLs. This may pose a security risk.\n\nContinue loading?"),
                                          QMessageBox::Ok | QMessageBox::Cancel);

        if (reply == QMessageBox::Ok)
        {
            accept = true;
        }
    }

    return accept;
}

QString WebWidget::getGlobalScript(QString authcode)
{
    if (PageGlobalScript.isEmpty())
    {
        QFile file(":/Resources/pageglobals.js");
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            throw std::runtime_error("pageglobal script load failure");
        }

        QTextStream in(&file);
        PageGlobalScript = in.readAll();
    }

    QSettings settings;
    bool      secure = settings.value(QUASAR_CONFIG_SECURE, QUASAR_DATA_SERVER_DEFAULT_SECURE).toBool();
    quint16   port   = settings.value(QUASAR_CONFIG_PORT, QUASAR_DATA_SERVER_DEFAULT_PORT).toUInt();

    QString pscript = PageGlobalScript.arg(secure ? "wss" : "ws").arg(port).arg(authcode);

    return pscript;
}

QString WebWidget::getFullPath()
{
    return data[WGT_DEF_FULLPATH].toString();
}

void WebWidget::saveSettings()
{
    QSettings settings;
    settings.setValue(getSettingKey("geometry"), saveGeometry());
    settings.setValue(getSettingKey("alwaysOnTop"), rOnTop->isChecked());
    settings.setValue(getSettingKey("fixedPosition"), m_fixedposition);
    settings.setValue(getSettingKey("clickable"), rClickable->isChecked());
}

void WebWidget::createContextMenuActions()
{
    rName   = new QAction(m_Name, this);
    QFont f = rName->font();
    f.setBold(true);
    rName->setFont(f);
    connect(rName, &QAction::triggered, [=](bool e) {
        QFileInfo info(this->getFullPath());
        QDesktopServices::openUrl(QUrl(info.absolutePath()));
    });

    rReload = new QAction(tr("&Reload"), this);
    connect(rReload, &QAction::triggered, [=] {
        if (webview->page()->scripts().size())
        {
            // Delete old script if it exists
            webview->page()->scripts().remove(script);

            // Insert refreshed script
            QString authcode    = server->generateAuthCode(m_Name);
            QString pageGlobals = getGlobalScript(authcode);

            script.setSourceCode(pageGlobals);

            webview->page()->scripts().insert(script);
        }
        webview->reload();
    });

    rResetPos = new QAction(tr("Re&set Position"), this);
    connect(rResetPos, &QAction::triggered, [=](bool e) { this->move(0, 0); });

    rOnTop = new QAction(tr("&Always on Top"), this);
    rOnTop->setCheckable(true);
    connect(rOnTop, &QAction::triggered, this, &WebWidget::toggleOnTop);

    rFixedPos = new QAction(tr("&Fixed Position"), this);
    rFixedPos->setCheckable(true);
    connect(rFixedPos, &QAction::triggered, [=](bool enabled) {
        m_fixedposition = enabled;

        QSettings settings;
        settings.setValue(getSettingKey("fixedPosition"), m_fixedposition);
    });

    rClickable = new QAction(tr("&Clickable"), this);
    rClickable->setCheckable(true);
    connect(rClickable, &QAction::triggered, [=](bool enabled) {
        overlay->setVisible(!enabled);

        QSettings settings;
        settings.setValue(getSettingKey("clickable"), enabled);
    });

    rClose = new QAction(tr("&Close"), this);
    connect(rClose, &QAction::triggered, [=] {
        // Remove from loaded
        QSettings   settings;
        QStringList loaded = settings.value(QUASAR_CONFIG_LOADED).toStringList();
        loaded.removeOne(getFullPath());
        settings.setValue(QUASAR_CONFIG_LOADED, loaded);

        emit WebWidgetClosed(this);
        close();
    });
}

void WebWidget::createContextMenu()
{
    m_Menu = new QMenu(m_Name, this);
    m_Menu->addAction(rName);
    m_Menu->addSeparator();
    m_Menu->addAction(rReload);
    m_Menu->addAction(rResetPos);
    m_Menu->addSeparator();
    m_Menu->addAction(rOnTop);
    m_Menu->addAction(rFixedPos);
    m_Menu->addAction(rClickable);
    m_Menu->addSeparator();
    m_Menu->addAction(rClose);
}

void WebWidget::mousePressEvent(QMouseEvent* evt)
{
    if (!m_fixedposition && evt->button() == Qt::LeftButton)
    {
        dragPosition = evt->globalPos() - frameGeometry().topLeft();
        evt->accept();
    }
}

void WebWidget::mouseMoveEvent(QMouseEvent* evt)
{
    if (!m_fixedposition && evt->buttons() & Qt::LeftButton)
    {
        move(evt->globalPos() - dragPosition);
        evt->accept();
    }
}

void WebWidget::toggleOnTop(bool ontop)
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

    QSettings settings;
    settings.setValue(getSettingKey("alwaysOnTop"), ontop);
}

QString WebWidget::getSettingKey(QString key)
{
    return m_Name + "/" + key;
}
