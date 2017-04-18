#pragma once

#include <QDialog>

QT_FORWARD_DECLARE_CLASS(QListWidget)
QT_FORWARD_DECLARE_CLASS(QListWidgetItem)
QT_FORWARD_DECLARE_CLASS(QStackedWidget)

class ConfigDialog : public QDialog
{
    Q_OBJECT

public:
    ConfigDialog(QObject* quasar);

public slots:
    void changePage(QListWidgetItem* current, QListWidgetItem* previous);

private slots:
    void saveSettings();

private:
    void createIcons();

    QListWidget*    contentsWidget;
    QStackedWidget* pagesWidget;
};
