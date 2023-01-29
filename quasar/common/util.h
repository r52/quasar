#pragma once

#include <tuple>
#include <vector>

#include <QPixmap>
#include <QString>

namespace Util
{
    QString                                    GetCommonAppDataPath();
    std::vector<std::string>                   SplitString(const std::string& src, const std::string& delimiter);

    [[nodiscard]] std::tuple<QPixmap, QString> ConvertB64ImageToPixmap(const std::string& image);
    [[nodiscard]] std::string                  ConvertPixmapToB64Image(const QPixmap& pixmap);

    char*                                      SafeCStrCopy(char* dest, size_t destSize, const char* src, size_t srcSize);
};  // namespace Util
