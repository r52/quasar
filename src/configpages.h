#pragma once

#include <QWidget>

QT_FORWARD_DECLARE_CLASS(Quasar)
QT_FORWARD_DECLARE_CLASS(DataPlugin)

class ConfigurationPage : public QWidget
{
public:
    ConfigurationPage(QObject *quasar, QWidget *parent = 0);

private slots:
    void pluginListClicked(QListWidgetItem *item);

private:
    Quasar *m_quasar;

    QLabel *plugName;
    QLabel *plugCode;
    QLabel *plugVersion;
    QLabel *plugAuthor;
    QLabel *plugDesc;
};

class PluginPage : public QWidget
{
public:
    PluginPage(QObject *quasar, QWidget *parent = 0);

private:
    Quasar *m_quasar;

    QStackedWidget *pagesWidget;
};

class DataPluginPage : public QWidget
{
public:
    DataPluginPage(DataPlugin *plugin, QWidget *parent = 0);
};
