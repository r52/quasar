#include "ajax.h"

#include "api/extension_support.hpp"

#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

#include <jsoncons/json.hpp>
#include <spdlog/spdlog.h>

namespace
{
    quasar_ext_handle    extHandle = nullptr;
    quasar_data_source_t sources[] = {
        { "get", QUASAR_POLLING_CLIENT, 0, 0},
        {"post", QUASAR_POLLING_CLIENT, 0, 0}
    };

    bool ajax_init(quasar_ext_handle handle)
    {
        extHandle = handle;
        return true;
    }

    bool ajax_shutdown(quasar_ext_handle handle)
    {
        return true;
    }

    bool ajax_get_data(size_t srcUid, quasar_data_handle hData, char* args)
    {
        if (srcUid != sources[0].uid and srcUid != sources[1].uid)
        {
            SPDLOG_WARN("Unknown source {}", srcUid);
            return false;
        }

        jsoncons::json jarg = args ? jsoncons::json::parse(args) : jsoncons::json();

        if (jarg.empty())
        {
            SPDLOG_WARN("Invalid args supplied to ajax", srcUid);
            SPDLOG_WARN("args: {}", args);
            return false;
        }

        if (!jarg.contains("url"))
        {
            SPDLOG_WARN("Missing URL in args supplied to ajax", srcUid);
            SPDLOG_WARN("args: {}", args);
            return false;
        }

        auto urlstr = jarg["url"].as_string();
        auto url    = QString::fromStdString(urlstr);

        jarg.erase("url");

        // Very wasteful, but since QNetworkAccessManager only supports usage from the thread it was created on,
        // this is the easy way to have it work with a threadpool
        QNetworkAccessManager manager;

        if (srcUid == sources[0].uid)
        {
            // get
            auto reply = manager.get(QNetworkRequest(QUrl(url)));

            // Just wait for the request to finish since all get_data requests
            // are ran on its own thread in a pool anyways
            QEventLoop loop;
            QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
            loop.exec();

            reply->deleteLater();

            if (reply->error() != QNetworkReply::NoError)
            {
                auto errstr = reply->errorString().toStdString();
                SPDLOG_WARN("AJAX: {} - {}", reply->error(), errstr);
                quasar_append_error(hData, errstr.c_str());
                return false;
            }

            auto data = reply->readAll();

            quasar_set_data_string_hpp(hData, data.toStdString());

            return true;
        }
        else if (srcUid == sources[1].uid)
        {
            // post
            std::string params{};
            jarg.dump(params);

            auto reply = manager.post(QNetworkRequest(QUrl(url)), QByteArray::fromStdString(params));

            // Just wait for the request to finish since all get_data requests
            // are ran on its own thread in a pool anyways
            QEventLoop loop;
            QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
            loop.exec();

            reply->deleteLater();

            if (reply->error() != QNetworkReply::NoError)
            {
                auto errstr = reply->errorString().toStdString();
                SPDLOG_WARN("AJAX: {} - {}", reply->error(), errstr);
                quasar_append_error(hData, errstr.c_str());
                return false;
            }

            quasar_set_data_null(hData);

            return true;
        }

        return false;
    }

    quasar_ext_info_fields_t fields = {"ajax", "AJAX Runner", "3.0", "r52", "Network access internal extension for Quasar", "https://github.com/r52/quasar"};

    quasar_ext_info_t        info   = {QUASAR_API_VERSION,
                 &fields,

                 std::size(sources),
                 sources,

                 ajax_init,      // init
                 ajax_shutdown,  // shutdown
                 ajax_get_data,  // data
                 nullptr,
                 nullptr};

}  // namespace

quasar_ext_info_t* ajax_load(void)
{
    return &info;
}

void ajax_destroy(quasar_ext_info_t* info) {}
