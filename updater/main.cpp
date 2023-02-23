#include <chrono>
#include <filesystem>
#include <thread>

#include <QCoreApplication>
#include <QProcess>
#include <QtDebug>

#include <mz.h>
#include <mz_strm.h>
#include <mz_zip.h>
#include <mz_zip_rw.h>

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QStringList      args(app.arguments());

    if (argc != 2)
    {
        // Too many or too few arguments
        qWarning() << "Usage: ./updater <package.zip>";
        return 1;
    }

    std::filesystem::path file = argv[1];

    if (!std::filesystem::exists(file))
    {
        qWarning() << "File" << file.string().c_str() << "does not exist";
        return 1;
    }

    qInfo() << "Unpacking" << file.string().c_str() << "...";

    std::this_thread::sleep_for(std::chrono::seconds(2));

    void*   reader    = nullptr;
    int32_t err       = MZ_OK;
    int32_t err_close = MZ_OK;

    mz_zip_reader_create(&reader);

    err = mz_zip_reader_open_file(reader, file.string().c_str());

    if (err != MZ_OK)
    {
        qWarning() << "Error" << err << "opening archive" << file.string().c_str();
    }
    else
    {
        err = mz_zip_reader_save_all(reader, ".");

        if (err != MZ_OK)
        {
            qWarning() << "Error" << err << "saving entries to disk" << file.string().c_str();
        }
    }

    err_close = mz_zip_reader_close(reader);
    if (err_close != MZ_OK)
    {
        qWarning() << "Error" << err_close << "closing archive for reading";
        err = err_close;
    }

    mz_zip_reader_delete(&reader);

    if (err == MZ_OK)
    {
        qInfo() << "Update complete! Starting Quasar...";

        std::this_thread::sleep_for(std::chrono::seconds(2));

        QProcess::startDetached("quasar.exe");
    }

    return err;
}
