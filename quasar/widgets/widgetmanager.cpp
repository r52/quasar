#include "widgetmanager.h"

#include "quasarwidget.h"

#include "common/config.h"
#include "common/settings.h"
#include "common/util.h"
#include "server/server.h"

#include <fstream>

#include <QMessageBox>
#include <QNetworkCookie>
#include <QWebEngineCookieStore>
#include <QWebEngineProfile>

#include <fmt/core.h>
#include <fmt/ranges.h>

#include <jsoncons/json.hpp>
#include <spdlog/spdlog.h>

JSONCONS_N_MEMBER_TRAITS(ExtensionRequirement, 2, name, platform);
JSONCONS_N_MEMBER_TRAITS(WidgetDefinition, 5, name, width, height, startFile, transparentBg, clickable, dataserver, remoteAccess, required);

#if defined(Q_OS_WIN)
constexpr auto _ostype = "windows";
#elif defined(Q_OS_LINUX)
constexpr auto _ostype = "linux";
#else
constexpr auto _ostype = "unknown";
#endif

namespace
{
    enum NetscapeCookieFormat
    {
        NETSCAPE_COOKIE_DOMAIN = 0,
        NETSCAPE_COOKIE_FLAG,
        NETSCAPE_COOKIE_PATH,
        NETSCAPE_COOKIE_SECURE,
        NETSCAPE_COOKIE_EXP,
        NETSCAPE_COOKIE_NAME,
        NETSCAPE_COOKIE_VALUE,
        NETSCAPE_COOKIE_MAX
    };
}  // namespace

WidgetManager::WidgetManager(std::shared_ptr<Server> serv, std::shared_ptr<Config> cfg) : server{serv}, config{cfg}
{
    auto cookiesfile = Settings::internal.cookies.GetValue();

    if (cookiesfile.empty())
    {
        SPDLOG_INFO("cookies.txt not set");
        return;
    }

    QFile cookiestxt(QString::fromStdString(cookiesfile));

    if (!cookiestxt.open(QIODevice::ReadOnly))
    {
        SPDLOG_WARN("Failed to load {}", cookiesfile);
    }
    else
    {
        QWebEngineCookieStore* store = QWebEngineProfile::defaultProfile()->cookieStore();

        // parse netscape format cookies file
        QTextStream in(&cookiestxt);

        while (!in.atEnd())
        {
            QString line = in.readLine();

            if (line.at(0) == '#')
            {
                // skip comments
                continue;
            }

            QStringList vals = line.split('\t');

            if (vals.count() != NETSCAPE_COOKIE_MAX)
            {
                // ill formatted line
                SPDLOG_WARN("Ill formatted cookie \"{}\"", line.toStdString());
                continue;
            }

            QNetworkCookie cookie(vals[NETSCAPE_COOKIE_NAME].toUtf8(), vals[NETSCAPE_COOKIE_VALUE].toUtf8());
            cookie.setDomain(vals[NETSCAPE_COOKIE_DOMAIN]);
            cookie.setExpirationDate(QDateTime::fromSecsSinceEpoch(vals[NETSCAPE_COOKIE_EXP].toLongLong()));
            cookie.setPath(vals[NETSCAPE_COOKIE_PATH]);
            cookie.setSecure(vals[NETSCAPE_COOKIE_SECURE] == "TRUE");

            store->setCookie(cookie);
        }

        SPDLOG_INFO("cookies.txt loaded");
    }
}

WidgetManager::~WidgetManager()
{
    widgetMap.clear();
}

bool WidgetManager::LoadWidget(const std::string& filename, bool userAction)
{
    if (filename.empty())
    {
        SPDLOG_ERROR("Error loading widget: Null filename");
        return false;
    }

    auto def_file = std::fstream(filename);

    if (!def_file)
    {
        SPDLOG_ERROR("Failed to load {}", filename);
        return false;
    }

    std::string json_doc;
    std::copy(std::istreambuf_iterator<char>(def_file), std::istreambuf_iterator<char>(), std::back_inserter(json_doc));

    WidgetDefinition def{};

    try
    {
        def = jsoncons::decode_json<WidgetDefinition>(json_doc);
    } catch (std::exception const& je)
    {
        SPDLOG_ERROR("Error parsing widget definition file '{}': {}", filename, je.what());
        SPDLOG_ERROR("JSON: {}", json_doc);
        return false;
    }

    def.fullpath = filename;

    // Verify extension dependencies
    if (def.required)
    {
        bool        pass    = true;
        std::string failext = "";

        auto        serv    = server.lock();

        for (auto p : def.required.value())
        {
            std::string extname;

            if (std::holds_alternative<std::string>(p))
            {
                extname = std::get<std::string>(p);
            }
            else if (std::holds_alternative<ExtensionRequirement>(p))
            {
                auto req = std::get<ExtensionRequirement>(p);

                if (req.platform != "windows" && req.platform != "linux")
                {
                    SPDLOG_WARN("Unrecognized platform requirement: {}", req.platform);
                    return false;
                }

                if (req.platform == _ostype)
                {
                    extname = req.name;
                }
                else
                {
                    continue;
                }
            }

            if (!serv->FindExtension(extname))
            {
                pass    = false;
                failext = extname;
                break;
            }
        }

        if (!pass)
        {
            SPDLOG_ERROR("Missing extension \"{}\" for widget \"{}\"", failext, filename);

            QMessageBox::warning(nullptr,
                QObject::tr("Missing Extension"),
                QObject::tr("Extension \"%1\" is required for widget \"%2\". Please install this extension and try again.")
                    .arg(QString::fromStdString(failext), QString::fromStdString(filename)),
                QMessageBox::Ok);

            return false;
        }
    }

    if (userAction and !acceptSecurityWarnings(def))
    {
        SPDLOG_WARN("Denied loading widget {}", filename);
        return false;
    }

    // Generate unique widget name
    std::string widgetName = def.name;
    int         idx        = 2;

    {
        std::unique_lock<std::shared_mutex> lk(mutex);

        while (widgetMap.count(widgetName) > 0)
        {
            widgetName = def.name + std::to_string(idx++);
        }

        SPDLOG_INFO("Loading widget \"{}\" ({})", widgetName, def.fullpath);

        auto widget = std::make_unique<QuasarWidget>(widgetName, def, server.lock(), shared_from_this(), config.lock());

        widget->show();

        if (userAction)
        {
            // Add to loaded
            auto loaded = getLoadedWidgetsList();
            loaded.push_back(widget->GetFullPath());
            saveLoadedWidgetsList(loaded);
        }

        widgetMap.insert(std::make_pair(widgetName, std::move(widget)));
    }

    if (widgetChangedCb)
    {
        widgetChangedCb(GetWidgets());
    }

    return true;
}

void WidgetManager::CloseWidget(QuasarWidget* widget)
{
    const auto name = widget->GetName();

    SPDLOG_INFO("Closing widget \"{}\" ({})", widget->GetName(), widget->GetFullPath());

    {
        std::unique_lock<std::shared_mutex> lk(mutex);

        // Remove from registry
        auto it = widgetMap.find(name);

        if (it != widgetMap.end())
        {
            assert((it->second.get()) == widget);
            // Release unique_ptr ownership to allow Qt gc to kick in
            (void) it->second.release();
            widgetMap.erase(it);
        }

        widget->deleteLater();
    }

    // Remove from loaded
    auto loaded = getLoadedWidgetsList();

    auto result = std::find(loaded.begin(), loaded.end(), widget->GetFullPath());
    if (result != loaded.end())
    {
        loaded.erase(result);
    }

    saveLoadedWidgetsList(loaded);

    if (widgetChangedCb)
    {
        widgetChangedCb(GetWidgets());
    }
}

void WidgetManager::LoadStartupWidgets()
{
    auto loaded = getLoadedWidgetsList();

    for (auto&& file : loaded)
    {
        LoadWidget(file, false);
    }
}

std::vector<QuasarWidget*> WidgetManager::GetWidgets()
{
    std::vector<QuasarWidget*> widgets;

    {
        std::shared_lock<std::shared_mutex> lk(mutex);

        std::transform(widgetMap.begin(), widgetMap.end(), std::back_inserter(widgets), [](auto& pair) {
            return pair.second.get();
        });
    }

    return widgets;
}

void WidgetManager::SetWidgetChangedCallback(WidgetChangedCallback&& cb)
{
    widgetChangedCb = std::move(cb);
}

bool WidgetManager::acceptSecurityWarnings(const WidgetDefinition& def)
{
    if (!def.remoteAccess.value_or(false))
    {
        return true;
    }

    auto reply = QMessageBox::warning(nullptr,
        QObject::tr("Remote Access"),
        QObject::tr("This widget requires remote access to external URLs. This may pose a security risk.\n\nContinue loading?"),
        QMessageBox::Ok | QMessageBox::Cancel);

    return (reply == QMessageBox::Ok);
}

std::vector<std::string> WidgetManager::getLoadedWidgetsList()
{
    auto loaded = Settings::internal.loaded_widgets.GetValue();

    return Util::SplitString<std::vector<std::string>>(loaded, ",");
}

void WidgetManager::saveLoadedWidgetsList(const std::vector<std::string>& list)
{
    auto amend = fmt::format("{}", fmt::join(list, ","));

    Settings::internal.loaded_widgets.SetValue(amend);
    config.lock()->Save();
}
