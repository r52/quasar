#include "util.h"

#include <regex>

#include <QStandardPaths>

QString Util::GetCommonAppDataPath()
{
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    path.truncate(path.lastIndexOf("/"));
    path.append("/");

    return path;
}

common_API std::vector<std::string> Util::SplitString(const std::string& src, const std::string& delimiter)
{
    const std::regex         del{","};
    std::vector<std::string> list(std::sregex_token_iterator(src.begin(), src.end(), del, -1), {});

    return list;
}
