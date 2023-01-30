#include "launchereditdialog.h"
#include "ui_launchereditdialog.h"

#include "common/qutil.h"

#include <QFileDialog>
#include <QImageReader>
#include <QMessageBox>

LauncherEditDialog::LauncherEditDialog(QString title, QWidget* parent, QString cmd, Settings::AppLauncherData data) :
    QDialog(parent),
    ui(new Ui::LauncherEditDialog)
{
    ui->setupUi(this);

    setWindowTitle(title);

    ui->fileEdit->setText(QString::fromStdString(data.file));
    ui->pathEdit->setText(QString::fromStdString(data.start));
    ui->argEdit->setText(QString::fromStdString(data.args));

    auto [pix, ib64] = QUtil::ConvertB64ImageToPixmap(data.icon);

    if (!pix.isNull())
    {
        ui->iconPix->setPixmap(pix);
    }

    // browse buttons
    connect(ui->fileBtn, &QPushButton::clicked, [=, this](bool checked) {
        QString filename = QFileDialog::getOpenFileName(this, tr("Choose Application"), QString(), tr("Executables (*.*)"));
        if (!filename.isEmpty())
        {
            QFileInfo info(filename);

            ui->fileEdit->setText(info.canonicalFilePath());
            ui->pathEdit->setText(info.canonicalPath());
        }
    });

    connect(ui->iconBtn, &QPushButton::clicked, [=, this](bool checked) {
        QFileDialog          dialog(this, tr("Choose Icon"));
        QStringList          mimeTypeFilters;
        const QByteArrayList supportedMimeTypes = QImageReader::supportedMimeTypes();
        for (const QByteArray& mimeTypeName : supportedMimeTypes)
            mimeTypeFilters.append(mimeTypeName);
        mimeTypeFilters.sort();
        dialog.setMimeTypeFilters(mimeTypeFilters);
        dialog.selectMimeTypeFilter("image/svg+xml");

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

            ui->iconPix->setPixmap(QPixmap::fromImage(newImage));
        }
    });

    // ok button
    connect(ui->buttonBox, &QDialogButtonBox::accepted, [=, this] {
        // validate
        if (ui->cmdEdit->text().isEmpty() || ui->fileEdit->text().isEmpty())
        {
            QMessageBox::warning(this, title, "Command and File Path must be filled out.");
        }
        else
        {
            command           = ui->cmdEdit->text();
            savedData.command = ui->cmdEdit->text().toStdString();
            savedData.file    = ui->fileEdit->text().toStdString();
            savedData.start   = ui->pathEdit->text().toStdString();
            savedData.args    = ui->argEdit->text().toStdString();
            savedData.icon    = QUtil::ConvertPixmapToB64Image(ui->iconPix->pixmap());
            accept();
        }
    });
}

LauncherEditDialog::~LauncherEditDialog()
{
    delete ui;
}
