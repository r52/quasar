#pragma once

#include <QString>

namespace Update
{
    enum UpdateStatus
    {
        NoUpdate = 0,
        HasUpdate,
        FinishedUpdating
    };

    UpdateStatus GetUpdateStatus(QString* filename = nullptr);
    bool         QueueUpdate(const QString& filename);
    void         RemoveUpdateQueue();
    void         CleanUpdateFiles();
    void         RunUpdate();
}  // namespace Update
