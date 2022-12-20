#include "util.h"

#include <QStandardPaths>

QString Util::GetCommonAppDataPath()
{
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    path.truncate(path.lastIndexOf("/"));
    path.append("/");

    return path;
}
