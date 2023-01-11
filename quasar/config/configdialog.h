#ifndef CONFIGDIALOG_H
#define CONFIGDIALOG_H

#include <QDialog>

class ExtensionPage;

namespace Ui
{
    class ConfigDialog;
}

class ConfigDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ConfigDialog(QWidget* parent = nullptr);
    ~ConfigDialog();

    void SaveSettings();

private:
    Ui::ConfigDialog*           ui;
    std::vector<ExtensionPage*> extensionPages;
};

#endif  // CONFIGDIALOG_H
