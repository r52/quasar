#include "update.h"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTextStream>

#include <spdlog/spdlog.h>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

namespace
{
    const QString _lockName = "_update.lock";

    bool          startProcess(const QString& cmd)
    {
#ifdef Q_OS_WIN
        STARTUPINFO         si;
        PROCESS_INFORMATION pi;

        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));

        // Start the child process.
        if (!CreateProcess(NULL,            // No module name (use command line)
                cmd.toStdWString().data(),  // Command line
                NULL,                       // Process handle not inheritable
                NULL,                       // Thread handle not inheritable
                FALSE,                      // Set handle inheritance to FALSE
                0,                          // No creation flags
                NULL,                       // Use parent's environment block
                NULL,                       // Use parent's starting directory
                &si,                        // Pointer to STARTUPINFO structure
                &pi)                        // Pointer to PROCESS_INFORMATION structure
        )
        {
            SPDLOG_WARN("CreateProcess failed {}", GetLastError());
            return false;
        }

        // Close process and thread handles.
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        return true;
#else
        return false;
#endif
    }
}  // namespace

Update::UpdateStatus Update::GetUpdateStatus(QString* filename)
{
#ifdef Q_OS_WIN
    QFile uplock(_lockName);
    if (!uplock.exists())
    {
        return Update::NoUpdate;
    }

    if (!uplock.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        SPDLOG_WARN("Failed to open update queue file");
        return Update::NoUpdate;
    }

    QTextStream in(&uplock);
    while (!in.atEnd())
    {
        QString file = in.readLine();
        if (file == "done")
        {
            return Update::FinishedUpdating;
        }

        QFile updPkg(file);
        if (updPkg.exists())
        {
            if (filename)
            {
                *filename = file;
            }
            return Update::HasUpdate;
        }
    }

    return Update::NoUpdate;
#else
    return Update::NoUpdate;
#endif
}

bool Update::QueueUpdate(const QString& filename)
{
    QFile uplock(_lockName);
    if (!uplock.open(QIODevice::ReadWrite | QIODeviceBase::Truncate | QIODevice::Text))
    {
        SPDLOG_WARN("Failed to open update queue file for writing");
        return false;
    }

    QTextStream out(&uplock);
    out << filename;

    return true;
}

void Update::RemoveUpdateQueue()
{
    QFile uplock(_lockName);
    if (uplock.exists())
    {
        uplock.remove();
    }
}

void Update::CleanUpdateFiles()
{
    // Clean up updater crud
    std::filesystem::path zlibold    = "zlib1-old.dll";
    std::filesystem::path updaterold = "updater-old.exe";

    if (std::filesystem::exists(zlibold))
    {
        SPDLOG_DEBUG("Cleaning up {}", zlibold.string());
        std::filesystem::remove(zlibold);
    }

    if (std::filesystem::exists(updaterold))
    {
        SPDLOG_DEBUG("Cleaning up {}", updaterold.string());
        std::filesystem::remove(updaterold);
    }

    auto exedir   = QDir();
    auto archives = exedir.entryList(QStringList() << "quasar-windows-*.zip", QDir::Files);

    for (auto&& pkg : archives)
    {
        SPDLOG_DEBUG("Cleaning up {}", pkg.toStdString());
        exedir.remove(pkg);
    }
}

void Update::RunUpdate()
{
#ifdef Q_OS_WIN
    QString updateFile;
    if (GetUpdateStatus(&updateFile) == Update::HasUpdate)
    {
        startProcess("updater.exe " + updateFile);
    }
#endif
}
