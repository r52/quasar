#pragma once

#include <QWidget>
#include <QSettings>

QT_FORWARD_DECLARE_CLASS(Quasar)
QT_FORWARD_DECLARE_CLASS(DataPlugin)
QT_FORWARD_DECLARE_CLASS(QListWidgetItem)
QT_FORWARD_DECLARE_CLASS(QLabel)
QT_FORWARD_DECLARE_CLASS(QStackedWidget)

class PageWidget : public QWidget
{
    Q_OBJECT

public:
    PageWidget(QWidget *parent = 0) : QWidget(parent) {}

    virtual void saveSettings(QSettings &settings, bool &restartNeeded) = 0;
};

class ConfigurationPage : public PageWidget
{
    Q_OBJECT

public:
    ConfigurationPage(QObject *quasar, QWidget *parent = 0);

    virtual void saveSettings(QSettings &settings, bool &restartNeeded) override;

private slots:
    void pluginListClicked(QListWidgetItem *item);

private:
    bool m_settingsModified = false;

    Quasar *m_quasar;

    QLabel *plugName;
    QLabel *plugCode;
    QLabel *plugVersion;
    QLabel *plugAuthor;
    QLabel *plugDesc;
};

class PluginPage : public PageWidget
{
    Q_OBJECT

public:
    PluginPage(QObject *quasar, QWidget *parent = 0);

    virtual void saveSettings(QSettings &settings, bool &restartNeeded) override;

private:
    Quasar *m_quasar;

    QStackedWidget *pagesWidget;
};

class DataPluginPage : public PageWidget
{
    Q_OBJECT

public:
    DataPluginPage(DataPlugin *p, QWidget *parent = 0);

    virtual void saveSettings(QSettings &settings, bool & restartNeeded) override;

private:
    bool m_dataSettingsModified = false;
    bool m_plugSettingsModified = false;

    DataPlugin *plugin;
};
