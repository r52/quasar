#pragma once

#include <QWidget>
#include <QtGui>

QT_FORWARD_DECLARE_CLASS(QWebEngineView);
QT_FORWARD_DECLARE_CLASS(QMenu);

// From https://stackoverflow.com/questions/19362455/dark-transparent-layer-over-a-qmainwindow-in-qt
class OverlayWidget : public QWidget
{
public:
    explicit OverlayWidget(QWidget* parent = 0)
        : QWidget(parent)
    {
        if (parent)
        {
            parent->installEventFilter(this);
            raise();
        }
    }

protected:
    //! Catches resize and child events from the parent widget
    bool eventFilter(QObject* obj, QEvent* ev)
    {
        if (obj == parent())
        {
            if (ev->type() == QEvent::Resize)
            {
                QResizeEvent* rev = static_cast<QResizeEvent*>(ev);
                resize(rev->size());
            }
            else if (ev->type() == QEvent::ChildAdded)
            {
                raise();
            }
        }
        return QWidget::eventFilter(obj, ev);
    }
    //! Tracks parent widget changes
    bool event(QEvent* ev)
    {
        if (ev->type() == QEvent::ParentAboutToChange)
        {
            if (parent())
                parent()->removeEventFilter(this);
        }
        else if (ev->type() == QEvent::ParentChange)
        {
            if (parent())
            {
                parent()->installEventFilter(this);
                raise();
            }
        }
        return QWidget::event(ev);
    }
};

class WebWidget : public QWidget
{
    Q_OBJECT;

public:
    explicit WebWidget(QString widgetName, const QJsonObject& dat, QWidget* parent = Q_NULLPTR);

    static bool validateWidgetDefinition(const QJsonObject& dat);
    static bool acceptSecurityWarnings(const QJsonObject& dat);

    QJsonObject getData() { return data; }
    QString     getName() { return m_Name; }
    QMenu*      getMenu() { return m_Menu; }

    QString getFullPath();

    void saveSettings();

signals:
    void WebWidgetClosed(WebWidget* widget);

protected:
    void createContextMenuActions();
    void createContextMenu();

    // Overrides
    virtual void mousePressEvent(QMouseEvent* evt) override;
    virtual void mouseMoveEvent(QMouseEvent* evt) override;
    virtual void contextMenuEvent(QContextMenuEvent* event) override;

    virtual void closeEvent(QCloseEvent* event) override;

protected slots:
    void toggleOnTop(bool ontop);

private:
    QString getWidgetConfigKey(QString key);

private:
    static QString PageGlobalTemp;

    bool m_fixedposition = false;

    QString m_Name;

    // Web engine widget
    QWebEngineView* webview;

    // Widget data
    QJsonObject data;

    // Drag and drop pos
    QPoint dragPosition;

    // Menu/actions
    QMenu*   m_Menu;
    QAction* rName;
    QAction* rReload;
    QAction* rResetPos;
    QAction* rOnTop;
    QAction* rFixedPos;
    QAction* rClose;
};
