#include "qutil.h"

#include <regex>

#include <QBuffer>
#include <QStandardPaths>

#include <fmt/core.h>

QString QUtil::GetCommonAppDataPath()
{
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    path.truncate(path.lastIndexOf("/"));
    path.append("/");

    return path;
}

std::tuple<QPixmap, QString> QUtil::ConvertB64ImageToPixmap(const std::string& image)
{
    if (image.empty())
    {
        return std::make_tuple(QPixmap(), QString());
    }

    auto imgraw  = std::regex_replace(image, std::regex("data:image/png;base64,"), "");

    auto iconb64 = QString::fromStdString(imgraw);
    auto icondat = QByteArray::fromBase64(iconb64.toLocal8Bit());
    auto iconimg = QImage::fromData(icondat);

    return std::make_tuple(QPixmap::fromImage(iconimg), QString::fromStdString(image));
}

std::string QUtil::ConvertPixmapToB64Image(const QPixmap& pixmap)
{
    if (pixmap.isNull())
    {
        return std::string{};
    }

    QByteArray bytes;
    QBuffer    buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    pixmap.save(&buffer, "PNG");

    auto image = fmt::format("{}{}", "data:image/png;base64,", bytes.toBase64().toStdString());

    return image;
}
