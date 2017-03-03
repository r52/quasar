#pragma once

#include <QtGui>
#include <QWidget>

QT_FORWARD_DECLARE_CLASS(QWebEngineView);

// From https://stackoverflow.com/questions/19362455/dark-transparent-layer-over-a-qmainwindow-in-qt
class OverlayWidget : public QWidget
{
public:
    explicit OverlayWidget(QWidget * parent = 0) : QWidget(parent) {
        if (parent) {
            parent->installEventFilter(this);
            raise();
        }
    }
protected:
    //! Catches resize and child events from the parent widget
    bool eventFilter(QObject * obj, QEvent * ev) {
        if (obj == parent()) {
            if (ev->type() == QEvent::Resize) {
                QResizeEvent * rev = static_cast<QResizeEvent*>(ev);
                resize(rev->size());
            }
            else if (ev->type() == QEvent::ChildAdded) {
                raise();
            }
        }
        return QWidget::eventFilter(obj, ev);
    }
    //! Tracks parent widget changes
    bool event(QEvent* ev) {
        if (ev->type() == QEvent::ParentAboutToChange) {
            if (parent()) parent()->removeEventFilter(this);
        }
        else if (ev->type() == QEvent::ParentChange) {
            if (parent()) {
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
    explicit WebWidget(const QJsonObject &dat, QWidget *parent = Q_NULLPTR);

    static bool validateWidgetDefinition(const QJsonObject &dat);
    static bool acceptSecurityWarnings(const QJsonObject &dat);

    QJsonObject getData()
    {
        return data;
    }

    void saveSettings();

signals:
    void WebWidgetClosed(WebWidget* widget);

protected:
    void createContextMenuActions();

    // Overrides
    virtual void mousePressEvent(QMouseEvent *evt) override;
    virtual void mouseMoveEvent(QMouseEvent *evt) override;
    virtual void contextMenuEvent(QContextMenuEvent *event) override;


    virtual void closeEvent(QCloseEvent *event) override;

protected slots:
    void toggleOnTop(bool ontop);

private:
    QString getWidgetConfigKey(QString key);

private:
    // Web engine widget
    QWebEngineView *webview;

    // Widget data
    QJsonObject data;

    // Drag and drop pos
    QPoint dragPosition;
    
    // Menu actions
    QAction *rName;
    QAction *rReload;
    QAction *rOnTop;
    QAction *rClose;
};
