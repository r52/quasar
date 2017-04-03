#pragma once

#include <QDialog>

class QListWidget;
class QListWidgetItem;
class QStackedWidget;

class ConfigDialog : public QDialog
{
    Q_OBJECT

public:
    ConfigDialog(QObject* quasar);

public slots:
    void changePage(QListWidgetItem *current, QListWidgetItem *previous);

private slots:
    void saveSettings();

private:
    void createIcons();

    QListWidget *contentsWidget;
    QStackedWidget *pagesWidget;
};
