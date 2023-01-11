#include "configdialog.h"
#include "ui_configdialog.h"

#include "extensionpage.h"
#include "launchereditdialog.h"

#include "common/settings.h"
#include "common/util.h"

#include <jsoncons/json.hpp>

JSONCONS_ALL_MEMBER_TRAITS(Settings::AppLauncherData, command, file, start, args, icon);

ConfigDialog::ConfigDialog(QWidget* parent) : QDialog(parent), ui(new Ui::ConfigDialog)
{
    ui->setupUi(this);

    // General settings
    ui->portSpin->setValue(Settings::internal.port.GetValue());
    ui->logCombo->setCurrentIndex(Settings::internal.log_level.GetValue());
    ui->logToFile->setChecked(Settings::internal.log_file.GetValue());

    // App launcher table
    auto applist = jsoncons::decode_json<std::vector<Settings::AppLauncherData>>(Settings::internal.applauncher.GetValue());

    {
        auto it  = applist.cbegin();
        int  row = 0;

        while (it != applist.cend())
        {
            auto& ref = *it;
            ui->appTable->insertRow(row);
            QTableWidgetItem* cmditem   = new QTableWidgetItem(QString::fromStdString(ref.command));
            QTableWidgetItem* fileitem  = new QTableWidgetItem(QString::fromStdString(ref.file));
            QTableWidgetItem* startitem = new QTableWidgetItem(QString::fromStdString(ref.start));
            QTableWidgetItem* argitem   = new QTableWidgetItem(QString::fromStdString(ref.args));

            auto [pix, ib64]            = Util::ConvertB64ImageToPixmap(ref.icon);
            QTableWidgetItem* iconitem  = new QTableWidgetItem(QIcon(pix), "");
            iconitem->setTextAlignment(Qt::AlignHCenter);
            iconitem->setData(Qt::UserRole, ib64);

            ui->appTable->setItem(row, 0, cmditem);
            ui->appTable->setItem(row, 1, fileitem);
            ui->appTable->setItem(row, 2, startitem);
            ui->appTable->setItem(row, 3, argitem);
            ui->appTable->setItem(row, 4, iconitem);

            ++row;

            ++it;
        }
    }

    connect(ui->deleteAppButton, &QPushButton::clicked, [=](bool checked) {
        int row = ui->appTable->currentRow();
        if (row >= 0)
        {
            ui->appTable->removeRow(ui->appTable->currentRow());
        }
    });

    connect(ui->editAppButton, &QPushButton::clicked, [=](bool checked) {
        bool ok;
        int  row = ui->appTable->currentRow();

        if (row >= 0)
        {
            QTableWidgetItem*         cmditem   = ui->appTable->item(row, 0);
            QTableWidgetItem*         fileitem  = ui->appTable->item(row, 1);
            QTableWidgetItem*         startitem = ui->appTable->item(row, 2);
            QTableWidgetItem*         argitem   = ui->appTable->item(row, 3);
            QTableWidgetItem*         iconitem  = ui->appTable->item(row, 4);

            Settings::AppLauncherData d{
                cmditem->text().toStdString(),
                fileitem->text().toStdString(),
                startitem->text().toStdString(),
                argitem->text().toStdString(),
                iconitem->data(Qt::UserRole).toString().toStdString(),
            };

            LauncherEditDialog* editdialog = new LauncherEditDialog("Edit App", this, cmditem->text(), d);
            connect(editdialog, &QDialog::finished, [=](int result) {
                if (result == QDialog::Accepted)
                {
                    auto& d = editdialog->GetData();

                    cmditem->setText(QString::fromStdString(d.command));
                    fileitem->setText(QString::fromStdString(d.file));
                    startitem->setText(QString::fromStdString(d.start));
                    argitem->setText(QString::fromStdString(d.args));

                    auto [pix, ib64] = Util::ConvertB64ImageToPixmap(d.icon);
                    iconitem->setIcon(QIcon(pix));
                    iconitem->setData(Qt::UserRole, ib64);
                }

                editdialog->deleteLater();
            });

            editdialog->open();
        }
    });

    connect(ui->addAppButton, &QPushButton::clicked, [=](bool checked) {
        LauncherEditDialog* editdialog = new LauncherEditDialog("New App", this);

        connect(editdialog, &QDialog::finished, [=](int result) {
            if (result == QDialog::Accepted)
            {
                auto& d   = editdialog->GetData();

                int   row = ui->appTable->rowCount();
                ui->appTable->insertRow(row);

                QTableWidgetItem* cmditem   = new QTableWidgetItem(QString::fromStdString(d.command));
                QTableWidgetItem* fileitem  = new QTableWidgetItem(QString::fromStdString(d.file));
                QTableWidgetItem* startitem = new QTableWidgetItem(QString::fromStdString(d.start));
                QTableWidgetItem* argitem   = new QTableWidgetItem(QString::fromStdString(d.args));

                auto [pix, ib64]            = Util::ConvertB64ImageToPixmap(d.icon);
                QTableWidgetItem* iconitem  = new QTableWidgetItem(QIcon(pix), "");
                iconitem->setTextAlignment(Qt::AlignHCenter);
                iconitem->setData(Qt::UserRole, ib64);

                ui->appTable->setItem(row, 0, cmditem);
                ui->appTable->setItem(row, 1, fileitem);
                ui->appTable->setItem(row, 2, startitem);
                ui->appTable->setItem(row, 3, argitem);
                ui->appTable->setItem(row, 4, iconitem);
            }

            editdialog->deleteLater();
        });

        editdialog->open();
    });

    // Extensions
    for (auto& [name, extset] : Settings::extension)
    {
        QListWidgetItem* extitem = new QListWidgetItem(ui->listWidget);
        extitem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        extitem->setText(QString::fromStdString(name));

        auto& [info, src, settings] = extset;
        auto page                   = new ExtensionPage(name, info, src, settings);
        extensionPages.push_back(page);
        ui->pagesWidget->addWidget(page);
    }

    connect(this, &QDialog::accepted, [=] {
        SaveSettings();
    });
}

ConfigDialog::~ConfigDialog()
{
    delete ui;
}

void ConfigDialog::SaveSettings()
{
    // General
    Settings::internal.port.SetValue(ui->portSpin->value());
    Settings::internal.log_level.SetValue(ui->logCombo->currentIndex());
    Settings::internal.log_file.SetValue(ui->logToFile->isChecked());

    // App table
    std::vector<Settings::AppLauncherData> applist;

    int                                    rows = ui->appTable->rowCount();

    for (int i = 0; i < rows; i++)
    {
        QTableWidgetItem*         cmditem   = ui->appTable->item(i, 0);
        QTableWidgetItem*         fileitem  = ui->appTable->item(i, 1);
        QTableWidgetItem*         startitem = ui->appTable->item(i, 2);
        QTableWidgetItem*         argitem   = ui->appTable->item(i, 3);
        QTableWidgetItem*         iconitem  = ui->appTable->item(i, 4);

        Settings::AppLauncherData d{
            cmditem->text().toStdString(),
            fileitem->text().toStdString(),
            startitem->text().toStdString(),
            argitem->text().toStdString(),
            iconitem->data(Qt::UserRole).toString().toStdString(),
        };

        applist.push_back(Settings::AppLauncherData{
            cmditem->text().toStdString(),
            fileitem->text().toStdString(),
            startitem->text().toStdString(),
            argitem->text().toStdString(),
            iconitem->data(Qt::UserRole).toString().toStdString(),
        });
    }

    std::string output{};
    jsoncons::encode_json(applist, output);
    Settings::internal.applauncher.SetValue(output);

    // TODO Extension settings save
}
