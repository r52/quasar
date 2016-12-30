#include "webwidget.h"
#include "widgetdefs.h"

#include <QAction>
#include <QMenu>

WebWidget::WebWidget(const QJsonObject &dat, QWidget *parent) : QWidget(parent)
{
    // Copy data
    data = dat;

    createContextMenuActions();

    // No frame/border, no taskbar button
    setWindowFlags(Qt::FramelessWindowHint | Qt::SubWindow);

    QWebEngineView *webview = new QWebEngineView(this);

    QString startFilePath = QFileInfo(data[WGT_DEF_FULLPATH].toString()).canonicalPath().append("/");
    QUrl startFile = QUrl::fromLocalFile(startFilePath.append(data[WGT_DEF_STARTFILE].toString()));

    webview->setUrl(startFile);

    // Optional background transparency
    if (data[WGT_DEF_TRANSPARENTBG].toBool())
    {
        setAttribute(Qt::WA_TranslucentBackground);
        webview->page()->setBackgroundColor(Qt::transparent);
    }

    webview->resize(data[WGT_DEF_WIDTH].toInt(), data[WGT_DEF_HEIGHT].toInt());

    // Overlay for catching drag and drop events
    OverlayWidget *overlay = new OverlayWidget(this);

    QSettings settings(STATE_CFG_FILENAME, QSettings::IniFormat);
    restoreGeometry(settings.value(getWidgetConfigKey("geometry")).toByteArray());
}

bool WebWidget::validateWidgetDefinition(const QJsonObject &dat)
{
    if (!dat.isEmpty() &&
        dat.contains(WGT_DEF_NAME) && !dat[WGT_DEF_NAME].toString().isNull() &&
        dat.contains(WGT_DEF_WIDTH) && dat[WGT_DEF_WIDTH].toInt() > 0 &&
        dat.contains(WGT_DEF_HEIGHT) && dat[WGT_DEF_HEIGHT].toInt() > 0 &&
        dat.contains(WGT_DEF_STARTFILE) && !dat[WGT_DEF_STARTFILE].toString().isNull() &&
        dat.contains(WGT_DEF_FULLPATH) && !dat[WGT_DEF_FULLPATH].toString().isNull())
    {
        return true;
    }

    return false;
}

void WebWidget::saveSettings()
{
    QSettings settings(STATE_CFG_FILENAME, QSettings::IniFormat);
    settings.setValue(getWidgetConfigKey("geometry"), saveGeometry());
}

void WebWidget::createContextMenuActions()
{
    rName = new QAction(data[WGT_DEF_NAME].toString());
    rName->font().setBold(true);

    rClose = new QAction(tr("Close"), this);
    connect(rClose, &QAction::triggered, this, &WebWidget::close);
}

void WebWidget::mousePressEvent(QMouseEvent *evt)
{
    oldPos = evt->globalPos();
}

void WebWidget::mouseMoveEvent(QMouseEvent *evt)
{
    const QPoint delta = evt->globalPos() - oldPos;
    move(x() + delta.x(), y() + delta.y());
    oldPos = evt->globalPos();
}

void WebWidget::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    menu.addAction(rName);
    menu.addAction(rClose);

    menu.exec(event->globalPos());
}

void WebWidget::closeEvent(QCloseEvent *event)
{
    saveSettings();

    emit WebWidgetClosed(this);
    event->accept();
}

QString WebWidget::getWidgetConfigKey(QString key)
{
    return data[WGT_DEF_NAME].toString() + "/" + key;
}
