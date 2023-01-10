#include "util.h"

#include <regex>

#include <QBuffer>
#include <QStandardPaths>

#include <fmt/core.h>

QString Util::GetCommonAppDataPath()
{
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    path.truncate(path.lastIndexOf("/"));
    path.append("/");

    return path;
}

std::vector<std::string> Util::SplitString(const std::string& src, const std::string& delimiter)
{
    const std::regex         del{","};
    std::vector<std::string> list(std::sregex_token_iterator(src.begin(), src.end(), del, -1), {});

    return list;
}

std::tuple<QPixmap, QString> Util::ConvertB64ImageToPixmap(const std::string& image)
{
    auto imgraw  = std::regex_replace(image, std::regex("data:image/png;base64,"), "");

    auto iconb64 = QString::fromStdString(imgraw);
    auto icondat = QByteArray::fromBase64(iconb64.toLocal8Bit());
    auto iconimg = QImage::fromData(icondat);

    return std::make_tuple(QPixmap::fromImage(iconimg), QString::fromStdString(image));
}

std::string Util::ConvertPixmapToB64Image(const QPixmap& pixmap)
{
    QByteArray bytes;
    QBuffer    buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    pixmap.save(&buffer, "PNG");

    auto image = fmt::format("{}{}", "data:image/png;base64,", bytes.toBase64().toStdString());

    return image;
}
