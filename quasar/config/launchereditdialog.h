#ifndef LAUNCHEREDITDIALOG_H
#define LAUNCHEREDITDIALOG_H

#include "common/settings.h"

#include <QDialog>

namespace Ui
{
    class LauncherEditDialog;
}

class LauncherEditDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LauncherEditDialog(QString title, QWidget* parent = nullptr, QString cmd = QString(), Settings::AppLauncherData data = {});
    ~LauncherEditDialog();

    QString&                   GetCommand() { return command; };

    Settings::AppLauncherData& GetData() { return savedData; };

private:
    Ui::LauncherEditDialog*   ui;

    QString                   command;
    Settings::AppLauncherData savedData;
};

#endif  // LAUNCHEREDITDIALOG_H
