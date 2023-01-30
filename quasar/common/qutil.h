#pragma once

#include <tuple>

#include <QPixmap>
#include <QString>

// Qt Utility functions
namespace QUtil
{
    QString                                    GetCommonAppDataPath();

    [[nodiscard]] std::tuple<QPixmap, QString> ConvertB64ImageToPixmap(const std::string& image);
    [[nodiscard]] std::string                  ConvertPixmapToB64Image(const QPixmap& pixmap);
}  // namespace QUtil
