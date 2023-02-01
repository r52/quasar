#pragma once

#include "common/settings.h"
#include "widgetdefinition.h"

#include <QtGui>
#include <QWebEngineScript>
#include <QWebEngineView>
#include <QWidget>

class Server;
class Config;
class WidgetManager;

// From https://stackoverflow.com/questions/19362455/dark-transparent-layer-over-a-qmainwindow-in-qt
class OverlayWidget : public QWidget
{
    void newParent()
    {
        if (!parent())
            return;
        parent()->installEventFilter(this);
        raise();
    }

public:
    explicit OverlayWidget(QWidget* parent = {}) : QWidget{parent}
    {
        setAttribute(Qt::WA_NoSystemBackground);
        newParent();
    }

protected:
    //! Catches resize and child events from the parent widget
    bool eventFilter(QObject* obj, QEvent* ev) override
    {
        if (obj == parent())
        {
            if (ev->type() == QEvent::Resize)
                resize(static_cast<QResizeEvent*>(ev)->size());
            else if (ev->type() == QEvent::ChildAdded)
                raise();
        }
        return QWidget::eventFilter(obj, ev);
    }

    //! Tracks parent widget changes
    bool event(QEvent* ev) override
    {
        if (ev->type() == QEvent::ParentAboutToChange)
        {
            if (parent())
                parent()->removeEventFilter(this);
        }
        else if (ev->type() == QEvent::ParentChange)
            newParent();
        return QWidget::event(ev);
    }
};

class QuasarWebView : public QWebEngineView
{
public:
    QuasarWebView(QWidget* parent = nullptr) : QWebEngineView{parent} { setContextMenuPolicy(Qt::NoContextMenu); }
};

class QuasarWebPage : public QWebEnginePage
{
public:
    QuasarWebPage(QObject* parent = nullptr) : QWebEnginePage{parent} {}

    QuasarWebPage(QWebEngineProfile* profile, QObject* parent = nullptr) : QWebEnginePage{profile, parent} {}

protected:
    virtual void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level, const QString& message, int lineNumber, const QString& sourceID);
    // bool         certificateError(const QWebEngineCertificateError& certificateError);
};

class QuasarWidget : public QWidget
{
    Q_OBJECT;

public:
    QuasarWidget(const QuasarWidget&)             = delete;
    QuasarWidget& operator= (const QuasarWidget&) = delete;

    QuasarWidget(const std::string&    widgetName,
        const WidgetDefinition&        def,
        std::shared_ptr<Server>        serv,
        std::shared_ptr<WidgetManager> man,
        std::shared_ptr<Config>        cfg);
    ~QuasarWidget();

    static QString     GetGlobalScript(const std::string& authcode);

    const std::string& GetFullPath() const { return widget_definition.fullpath; };

    const std::string& GetName() const { return name; };

    QMenu*             GetContextMenu() const { return contextMenu; }

    void               SaveSettings();

protected:
    void createContextMenuActions();
    void createContextMenu();

    // Overrides
    virtual void mousePressEvent(QMouseEvent* evt) override;
    virtual void mouseMoveEvent(QMouseEvent* evt) override;

protected slots:
    void toggleOnTop(bool ontop);

private:
    static QString GlobalScript;

    std::string    name;

    // Web engine widget
    QuasarWebView* webview{};

    // Widget overlay
    OverlayWidget* overlay{};

    // Page script
    QWebEngineScript script;

    // Drag and drop pos
    QPoint dragPosition{};

    // Context menu and actions
    QMenu*   contextMenu{};
    QAction* aName{};
    QAction* aReload{};
    QAction* aSetPos{};
    QAction* aResetPos{};
    QAction* aOnTop{};
    QAction* aFixedPos{};
    QAction* aClickable{};
    QAction* aResize{};
    QAction* aClose{};

    // Data
    WidgetDefinition             widget_definition{};
    Settings::WidgetSettings     settings{};
    std::weak_ptr<Server>        server{};
    std::weak_ptr<WidgetManager> manager{};
    std::weak_ptr<Config>        config{};
};
