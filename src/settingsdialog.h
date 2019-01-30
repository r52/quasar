#pragma once

#include <QWidget>
#include <QtWebEngineWidgets/QWebEngineProfile>

class SettingsDialog : public QWidget
{
public:
    SettingsDialog(QWidget* parent = nullptr);
    ~SettingsDialog();

private:
    SettingsDialog(const SettingsDialog&) = delete;
    SettingsDialog& operator=(const SettingsDialog&) = delete;

    QWebEngineProfile* profile;
};
