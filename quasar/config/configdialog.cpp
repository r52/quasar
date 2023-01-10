#include "configdialog.h"
#include "ui_configdialog.h"

#include "common/settings.h"
#include "common/util.h"

#include <QFileDialog>
#include <QImageReader>
#include <QLineEdit>
#include <QMessageBox>

#include <jsoncons/json.hpp>

JSONCONS_ALL_MEMBER_TRAITS(Settings::AppLauncherData, command, file, start, args, icon);

class LauncherEditDialog : public QDialog
{
public:
    LauncherEditDialog(QString title, QWidget* parent = Q_NULLPTR, QString command = QString(), Settings::AppLauncherData data = {}) : QDialog(parent)
    {
        setMinimumWidth(400);
        setWindowTitle(title);

        QLabel*    cmdLabel = new QLabel(tr("Launcher Command:"));
        QLineEdit* cmdEdit  = new QLineEdit;
        cmdEdit->setText(command);

        QHBoxLayout* cmdlayout = new QHBoxLayout;
        cmdlayout->addWidget(cmdLabel);
        cmdlayout->addWidget(cmdEdit);

        QLabel*      fileLabel = new QLabel(tr("File/Commandline:"));
        QLineEdit*   fileEdit  = new QLineEdit;
        QPushButton* fileBtn   = new QPushButton(tr("Browse"));
        fileEdit->setText(QString::fromStdString(data.file));

        QHBoxLayout* filelayout = new QHBoxLayout;
        filelayout->addWidget(fileLabel);
        filelayout->addWidget(fileEdit);
        filelayout->addWidget(fileBtn);

        QLabel*    pathLabel = new QLabel(tr("Start Path:"));
        QLineEdit* pathEdit  = new QLineEdit;
        pathEdit->setText(QString::fromStdString(data.start));

        QHBoxLayout* pathlayout = new QHBoxLayout;
        pathlayout->addWidget(pathLabel);
        pathlayout->addWidget(pathEdit);

        QLabel*    argLabel = new QLabel(tr("Arguments:"));
        QLineEdit* argEdit  = new QLineEdit;
        argEdit->setText(QString::fromStdString(data.args));

        QHBoxLayout* arglayout = new QHBoxLayout;
        arglayout->addWidget(argLabel);
        arglayout->addWidget(argEdit);

        QLabel*      iconLabel = new QLabel(tr("Icon:"));
        QLabel*      iconPix   = new QLabel;
        QPushButton* iconBtn   = new QPushButton(tr("Browse"));

        auto [pix, ib64]       = Util::ConvertB64ImageToPixmap(data.icon);

        if (!pix.isNull())
        {
            iconPix->setPixmap(pix);
        }

        QHBoxLayout* iconlayout = new QHBoxLayout;
        iconlayout->addWidget(iconLabel);
        iconlayout->addWidget(iconPix);
        iconlayout->addWidget(iconBtn);

        QPushButton* okBtn     = new QPushButton(tr("OK"));
        QPushButton* cancelBtn = new QPushButton(tr("Cancel"));

        okBtn->setDefault(true);

        QHBoxLayout* btnlayout = new QHBoxLayout;
        btnlayout->addWidget(okBtn);
        btnlayout->addWidget(cancelBtn);

        QVBoxLayout* layout = new QVBoxLayout;
        layout->addLayout(cmdlayout);
        layout->addLayout(filelayout);
        layout->addLayout(pathlayout);
        layout->addLayout(arglayout);
        layout->addLayout(iconlayout);
        layout->addLayout(btnlayout);

        // browse buttons
        connect(fileBtn, &QPushButton::clicked, [=](bool checked) {
            QString filename = QFileDialog::getOpenFileName(this, tr("Choose Application"), QString(), tr("Executables (*.*)"));
            if (!filename.isEmpty())
            {
                QFileInfo info(filename);

                fileEdit->setText(info.canonicalFilePath());
                pathEdit->setText(info.canonicalPath());
            }
        });

        connect(iconBtn, &QPushButton::clicked, [=](bool checked) {
            QFileDialog          dialog(this, tr("Choose Icon"));
            QStringList          mimeTypeFilters;
            const QByteArrayList supportedMimeTypes = QImageReader::supportedMimeTypes();
            for (const QByteArray& mimeTypeName : supportedMimeTypes)
                mimeTypeFilters.append(mimeTypeName);
            mimeTypeFilters.sort();
            dialog.setMimeTypeFilters(mimeTypeFilters);
            dialog.selectMimeTypeFilter("image/jpeg");

            if (dialog.exec() == QDialog::Accepted)
            {
                const auto   filename = dialog.selectedFiles().constFirst();
                QImageReader reader(filename);
                reader.setAutoTransform(true);
                const QImage newImage = reader.read();
                if (newImage.isNull())
                {
                    QMessageBox::information(this,
                        QGuiApplication::applicationDisplayName(),
                        tr("Cannot load %1: %2").arg(QDir::toNativeSeparators(filename), reader.errorString()));
                    return;
                }

                iconPix->setPixmap(QPixmap::fromImage(newImage));
            }
        });

        // cancel button
        connect(cancelBtn, &QPushButton::clicked, [=](bool checked) {
            close();
        });

        // ok button
        connect(okBtn, &QPushButton::clicked, [=](bool checked) {
            // validate
            if (cmdEdit->text().isEmpty() || fileEdit->text().isEmpty())
            {
                QMessageBox::warning(this, title, "Command and File Path must be filled out.");
            }
            else
            {
                m_command      = cmdEdit->text();
                m_data.command = cmdEdit->text().toStdString();
                m_data.file    = fileEdit->text().toStdString();
                m_data.start   = pathEdit->text().toStdString();
                m_data.args    = argEdit->text().toStdString();
                m_data.icon    = Util::ConvertPixmapToB64Image(iconPix->pixmap());
                accept();
            }
        });

        setLayout(layout);
    };

    QString&                   getCommand() { return m_command; };

    Settings::AppLauncherData& getData() { return m_data; };

private:
    QString                   m_command;
    Settings::AppLauncherData m_data;
};

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
                    auto& d = editdialog->getData();

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
                auto& d   = editdialog->getData();

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
}
