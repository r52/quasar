#pragma once

#include <QSettings>
#include <QWidget>

QT_FORWARD_DECLARE_CLASS(DataPlugin)
QT_FORWARD_DECLARE_CLASS(QListWidgetItem)
QT_FORWARD_DECLARE_CLASS(QLabel)
QT_FORWARD_DECLARE_CLASS(QStackedWidget)
QT_FORWARD_DECLARE_CLASS(DataServices)

class PageWidget : public QWidget
{
    Q_OBJECT

public:
    PageWidget(QWidget* parent = 0)
        : QWidget(parent) {}

    virtual void saveSettings(QSettings& settings, bool& restartNeeded) = 0;
};

class GeneralPage : public PageWidget
{
    Q_OBJECT

public:
    GeneralPage(DataServices* service, QWidget* parent = 0);

    virtual void saveSettings(QSettings& settings, bool& restartNeeded) override;

private slots:
    void pluginListClicked(QListWidgetItem* item);

private:
    bool m_settingsModified = false;

    DataServices* m_service;

    QLabel* plugName;
    QLabel* plugCode;
    QLabel* plugVersion;
    QLabel* plugAuthor;
    QLabel* plugDesc;
};

class PluginPage : public PageWidget
{
    Q_OBJECT

public:
    PluginPage(DataServices* service, QWidget* parent = 0);

    virtual void saveSettings(QSettings& settings, bool& restartNeeded) override;

private:
    DataServices* m_service;

    QStackedWidget* pagesWidget;
};

class DataPluginPage : public PageWidget
{
    Q_OBJECT

public:
    DataPluginPage(DataPlugin* p, QWidget* parent = 0);

    virtual void saveSettings(QSettings& settings, bool& restartNeeded) override;

private:
    bool m_dataSettingsModified = false;
    bool m_plugSettingsModified = false;

    DataPlugin* plugin;
};

class LauncherPage : public PageWidget
{
public:
    LauncherPage(DataServices* service, QWidget* parent = 0);

    virtual void saveSettings(QSettings& settings, bool& restartNeeded) override;

private:
    DataServices* m_service;
};
